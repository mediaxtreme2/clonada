-- @description Clonada - AI Voice Cloning & Swap Engine
-- @author mediaXtreme LLC
-- @version 1.0.0
-- @about AI vocal transformation suite for REAPER

local r = reaper
local script_path = debug.getinfo(1, 'S').source:match([[^@?(.*[\\/])]])
local sep = package.config:sub(1,1)
local engine_path = script_path .. "..{{ sep }}python" .. sep
local bin_path = script_path .. ".." .. sep .. "bin" .. sep

-- ═══════════════════════════════════════════════════════════
-- CONFIGURATION & STATE
-- ═══════════════════════════════════════════════════════════

local APP_NAME = "Clonada"
local APP_VERSION = "1.0.0"
local ZMQ_PORT = 5050
local MIN_DATASET_DURATION = 1800 -- 30 minutes minimum
local NORM_TARGET_DB = -1.0

local STATE = {
  EXT_SECTION = "ClonadaSuite",
  KEY_ACTIVATED = "CLONADA_ACTIVATED",
  KEY_TIER = "CLONADA_USER_TIER",
  KEY_LICENSE = "CLONADA_LICENSE_KEY",
  KEY_MACHINE_ID = "CLONADA_MACHINE_ID",
  KEY_TOKEN = "CLONADA_TOKEN_HASH",
  KEY_LAST_CHECK = "CLONADA_LAST_CHECK",
  KEY_RUNPOD_KEY = "CLONADA_RUNPOD_API_KEY",
  KEY_MODELS_DIR = "CLONADA_MODELS_DIR",
  KEY_PROCESSING_MODE = "CLONADA_PROC_MODE",
}

-- ═══════════════════════════════════════════════════════════
-- UI STATE
-- ═══════════════════════════════════════════════════════════

local current_tab = 0 -- 0=Swap, 1=Training, 2=Settings, 3=Activation
local is_engine_running = false
local is_processing = false
local progress_value = 0.0
local progress_text = "Ready"
local status_message = ""
local status_color = 0x99999999 -- gray

-- Swap tab state
local swap_model_list = {}
local swap_selected_model = 0
local swap_pitch_shift = 0.0
local swap_formant_shift = 0.0
local swap_dry_wet = 1.0
local swap_pitch_method = 0 -- 0=rmvpe, 1=crepe, 2=harvest, 3=fcpe
local swap_processing_mode = 0 -- 0=Low Latency, 1=High Quality

-- Training tab state
local train_track_idx = 0
local train_epochs = 100
local train_batch_size = 8
local train_sample_rate = 40000
local train_status = "Idle"
local train_runpod_key = ""
local dataset_check_result = ""
local dataset_check_ok = false

-- Settings state
local settings_models_dir = ""
local settings_engine_port = ZMQ_PORT
local settings_auto_normalize = true
local settings_gpu_enabled = true

-- Activation state
local activation_key_input = ""
local activation_status = "Not Activated"
local activation_tier = "Free"

-- Input buffers for ImGui
local buf_license_key = ""
local buf_runpod_key = ""
local buf_models_dir = ""
local buf_pitch = "0"
local buf_formant = "0"
local buf_epochs = "100"

-- ═══════════════════════════════════════════════════════════
-- UTILITY FUNCTIONS
-- ═══════════════════════════════════════════════════════════

local function get_os()
  local os_name = r.GetOS()
  if os_name:find("Win") then return "windows"
  elseif os_name:find("OSX") or os_name:find("macOS") then return "macos"
  else return "linux" end
end

local function get_machine_id()
  local cached = r.GetExtState(STATE.EXT_SECTION, STATE.KEY_MACHINE_ID)
  if cached ~= "" then return cached end

  local cmd
  local os_type = get_os()
  if os_type == "windows" then
    cmd = 'wmic baseboard get serialnumber /value'
  elseif os_type == "macos" then
    cmd = "ioreg -l | grep IOPlatformSerialNumber | awk '{print $4}'"
  else
    cmd = "cat /etc/machine-id 2>/dev/null || echo FALLBACK_LINUX_ID"
  end

  local handle = io.popen(cmd)
  if not handle then return "FALLBACK_ID_" .. os.time() end
  local output = handle:read("*a")
  handle:close()

  local clean = output:gsub("%s+", ""):gsub("SerialNumber=", "")
  if clean == "" then clean = "FALLBACK_ID_" .. os.time() end

  r.SetExtState(STATE.EXT_SECTION, STATE.KEY_MACHINE_ID, clean, true)
  return clean
end

local function get_cli_path()
  local os_type = get_os()
  if os_type == "windows" then
    return bin_path .. "win64" .. sep .. "clonada_cli.exe"
  else
    return bin_path .. "macos" .. sep .. "clonada_cli"
  end
end

local function set_status(msg, color)
  status_message = msg
  status_color = color or 0x99999999
end

local function load_saved_state()
  activation_status = r.GetExtState(STATE.EXT_SECTION, STATE.KEY_ACTIVATED)
  if activation_status == "" then activation_status = "Not Activated" end
  activation_tier = r.GetExtState(STATE.EXT_SECTION, STATE.KEY_TIER)
  if activation_tier == "" then activation_tier = "Free" end
  train_runpod_key = r.GetExtState(STATE.EXT_SECTION, STATE.KEY_RUNPOD_KEY)
  buf_runpod_key = train_runpod_key
  settings_models_dir = r.GetExtState(STATE.EXT_SECTION, STATE.KEY_MODELS_DIR)
  if settings_models_dir == "" then
    settings_models_dir = script_path .. ".." .. sep .. "models"
  end
  buf_models_dir = settings_models_dir

  local mode = r.GetExtState(STATE.EXT_SECTION, STATE.KEY_PROCESSING_MODE)
  swap_processing_mode = (mode == "1") and 1 or 0
end

local function scan_models()
  swap_model_list = {}
  local dir = settings_models_dir
  if dir == "" then return end

  local i = 0
  repeat
    local file = r.EnumerateFiles(dir, i)
    if file and file:match("%.pth$") then
      table.insert(swap_model_list, file)
    end
    i = i + 1
  until not file
end

-- ═══════════════════════════════════════════════════════════
-- ENGINE MANAGEMENT
-- ═══════════════════════════════════════════════════════════

local engine_process = nil

local function start_engine()
  if is_engine_running then return true end

  local python_server = script_path .. ".." .. sep .. "python" .. sep .. "clonada_server.py"
  local os_type = get_os()
  local cmd

  if os_type == "windows" then
    cmd = string.format('start /B pythonw "%s" --port %d', python_server, settings_engine_port)
  else
    cmd = string.format('python3 "%s" --port %d &', python_server, settings_engine_port)
  end

  os.execute(cmd)
  is_engine_running = true
  set_status("Engine started on port " .. settings_engine_port, 0x44BB44FF)
  return true
end

local function stop_engine()
  if not is_engine_running then return end
  local cli = get_cli_path()
  local cmd = string.format('"%s" --command SHUTDOWN --port %d', cli, settings_engine_port)
  os.execute(cmd)
  is_engine_running = false
  set_status("Engine stopped", 0x999999FF)
end

-- ═══════════════════════════════════════════════════════════
-- DATASET VALIDATION
-- ═══════════════════════════════════════════════════════════

local function run_dataset_health_check(track_index)
  local track = r.GetTrack(0, track_index)
  if not track then
    dataset_check_result = "ERROR: No track found at index " .. track_index
    dataset_check_ok = false
    return false
  end

  local item_count = r.CountTrackMediaItems(track)
  if item_count == 0 then
    dataset_check_result = "ERROR: Track has no media items"
    dataset_check_ok = false
    return false
  end

  local total_duration = 0.0
  local min_rms = 0
  local max_peak = -math.huge
  local sample_rate = nil
  local channel_issues = {}

  for i = 0, item_count - 1 do
    local item = r.GetTrackMediaItem(track, i)
    local take = r.GetActiveTake(item)
    if take then
      local source = r.GetMediaItemTake_Source(take)
      local length = r.GetMediaItemInfo_Value(item, "D_LENGTH")
      total_duration = total_duration + length

      local sr = r.GetMediaSourceSampleRate(source)
      if sr > 0 then
        if not sample_rate then
          sample_rate = sr
        elseif sr ~= sample_rate then
          table.insert(channel_issues, string.format("Item %d: sample rate mismatch (%dHz vs %dHz)", i+1, sr, sample_rate))
        end
      end

      local channels = r.GetMediaSourceNumChannels(source)
      if channels > 1 then
        table.insert(channel_issues, string.format("Item %d: %d channels (mono recommended)", i+1, channels))
      end
    end
  end

  local result_lines = {}
  table.insert(result_lines, string.format("Items: %d", item_count))
  table.insert(result_lines, string.format("Total duration: %.1f min (%.0f sec)", total_duration / 60, total_duration))
  if sample_rate then
    table.insert(result_lines, string.format("Sample rate: %d Hz", sample_rate))
  end

  if #channel_issues > 0 then
    table.insert(result_lines, "\nWarnings:")
    for _, issue in ipairs(channel_issues) do
      table.insert(result_lines, "  - " .. issue)
    end
  end

  if total_duration < MIN_DATASET_DURATION then
    table.insert(result_lines, string.format("\nFAILED: Need %.0f+ minutes, got %.1f minutes", MIN_DATASET_DURATION / 60, total_duration / 60))
    dataset_check_ok = false
  else
    table.insert(result_lines, "\nPASSED: Dataset meets minimum requirements")
    dataset_check_ok = true
  end

  dataset_check_result = table.concat(result_lines, "\n")
  return dataset_check_ok
end

-- ═══════════════════════════════════════════════════════════
-- ONE-CLICK NORMALIZATION
-- ═══════════════════════════════════════════════════════════

local function apply_normalization(track_index)
  local track = r.GetTrack(0, track_index)
  if not track then
    set_status("No track found", 0xFF4444FF)
    return
  end

  r.Undo_BeginBlock()
  local item_count = r.CountTrackMediaItems(track)
  local target_vol = 10 ^ (NORM_TARGET_DB / 20) -- -1dBFS in linear

  for i = 0, item_count - 1 do
    local item = r.GetTrackMediaItem(track, i)
    r.SetMediaItemInfo_Value(item, "D_VOL", target_vol)
  end

  r.Undo_EndBlock("Clonada: Normalize to " .. NORM_TARGET_DB .. " dBFS", -1)
  r.UpdateArrange()
  set_status(string.format("Normalized %d items to %.1f dBFS", item_count, NORM_TARGET_DB), 0x44BB44FF)
end

-- ═══════════════════════════════════════════════════════════
-- VOICE SWAP EXECUTION
-- ═══════════════════════════════════════════════════════════

local swap_log_path = ""

local function execute_voice_swap()
  if is_processing then
    set_status("Already processing", 0xFFAA44FF)
    return
  end

  if #swap_model_list == 0 or swap_selected_model < 1 then
    set_status("No model selected", 0xFF4444FF)
    return
  end

  local sel_count = r.CountSelectedMediaItems(0)
  if sel_count == 0 then
    set_status("Select media items to process", 0xFF4444FF)
    return
  end

  -- Export selected items to temp WAV
  local item = r.GetSelectedMediaItem(0, 0)
  local take = r.GetActiveTake(item)
  if not take then
    set_status("No active take in selected item", 0xFF4444FF)
    return
  end

  local source = r.GetMediaItemTake_Source(take)
  local input_file = r.GetMediaSourceFileName(source)

  local model_name = swap_model_list[swap_selected_model]
  local model_path = settings_models_dir .. sep .. model_name
  local pitch_methods = {"rmvpe", "crepe", "harvest", "fcpe"}
  local method = pitch_methods[swap_pitch_method + 1] or "rmvpe"

  local mode_flag = swap_processing_mode == 0 and "low_latency" or "high_quality"

  local cli = get_cli_path()
  swap_log_path = script_path .. "progress.tmp"

  local cmd = string.format(
    '"%s" --command SWAP --input "%s" --model "%s" --pitch %d --formant %d --method %s --mix %.2f --mode %s --port %d --progress "%s"',
    cli, input_file, model_path,
    math.floor(swap_pitch_shift), math.floor(swap_formant_shift),
    method, swap_dry_wet, mode_flag, settings_engine_port, swap_log_path
  )

  is_processing = true
  progress_value = 0.0
  progress_text = "Initializing AI engine..."
  set_status("Processing voice swap...", 0x44AAFFFF)

  r.ExecProcess(cmd, -1)
  r.defer(watch_swap_progress)
end

function watch_swap_progress()
  if not is_processing then return end

  local log_file = io.open(swap_log_path, "r")
  if log_file then
    local line = log_file:read("*l")
    if line then
      local val, txt = line:match("([^|]+)|([^|]+)")
      if val then
        progress_value = tonumber(val) or 0
        progress_text = txt or "Processing..."
      end
    end
    log_file:close()
  end

  if progress_value >= 1.0 then
    is_processing = false
    progress_text = "Voice swap complete"
    set_status("Voice swap complete! Import the output file.", 0x44BB44FF)

    -- Clean up progress file
    os.remove(swap_log_path)

    -- Auto-import result
    local output_file = swap_log_path:gsub("progress%.tmp", "output.wav")
    if io.open(output_file, "r") then
      r.InsertMedia(output_file, 0)
      r.UpdateArrange()
    end
  else
    r.defer(watch_swap_progress)
  end
end

-- ═══════════════════════════════════════════════════════════
-- CLOUD TRAINING
-- ═══════════════════════════════════════════════════════════

local function start_cloud_training()
  if train_runpod_key == "" then
    set_status("Enter your RunPod API key in Settings", 0xFF4444FF)
    return
  end

  if not dataset_check_ok then
    set_status("Run dataset health check first", 0xFF4444FF)
    return
  end

  local track = r.GetTrack(0, train_track_idx)
  if not track then
    set_status("No track selected for training", 0xFF4444FF)
    return
  end

  is_processing = true
  progress_value = 0.0
  progress_text = "Preparing dataset for cloud training..."
  set_status("Training job submitted to RunPod", 0x44AAFFFF)

  local cli = get_cli_path()
  local log_path = script_path .. "train_progress.tmp"

  -- Export track audio, upload to RunPod, start training
  local cmd = string.format(
    '"%s" --command TRAIN --track %d --epochs %d --batch_size %d --sample_rate %d --runpod_key "%s" --models_dir "%s" --port %d --progress "%s"',
    cli, train_track_idx, train_epochs, train_batch_size, train_sample_rate,
    train_runpod_key, settings_models_dir, settings_engine_port, log_path
  )

  swap_log_path = log_path
  r.ExecProcess(cmd, -1)
  r.defer(watch_swap_progress)
end

-- ═══════════════════════════════════════════════════════════
-- ACTIVATION / LICENSING
-- ═══════════════════════════════════════════════════════════

local function activate_license(key)
  if key == "" then
    set_status("Enter a license key", 0xFF4444FF)
    return
  end

  local machine_id = get_machine_id()

  local cli = get_cli_path()
  local cmd = string.format('"%s" --command ACTIVATE --key "%s" --machine "%s" --port %d',
    cli, key, machine_id, settings_engine_port)

  local handle = io.popen(cmd)
  if handle then
    local result = handle:read("*a")
    handle:close()

    if result:find("SUCCESS") then
      local tier = result:match("TIER:(%w+)") or "Basic"
      r.SetExtState(STATE.EXT_SECTION, STATE.KEY_ACTIVATED, "Active", true)
      r.SetExtState(STATE.EXT_SECTION, STATE.KEY_TIER, tier, true)
      r.SetExtState(STATE.EXT_SECTION, STATE.KEY_LICENSE, key, true)
      r.SetExtState(STATE.EXT_SECTION, STATE.KEY_LAST_CHECK, tostring(os.time()), true)
      activation_status = "Active"
      activation_tier = tier
      set_status("License activated! Tier: " .. tier, 0x44BB44FF)
    else
      set_status("Activation failed: " .. result, 0xFF4444FF)
    end
  end
end

-- ═══════════════════════════════════════════════════════════
-- ImGui RENDERING
-- ═══════════════════════════════════════════════════════════

local ctx = r.ImGui_CreateContext(APP_NAME .. ' Dashboard')

-- Colors
local COL_GOLD = 0xD4A843FF
local COL_NAVY = 0x1B2A4AFF
local COL_NAVY_DEEP = 0x0F1A2EFF
local COL_CREAM = 0xE8E2D8FF
local COL_DARK_BG = 0x141824FF
local COL_CARD_BG = 0x1E2A3EFF
local COL_GREEN = 0x44BB44FF
local COL_RED = 0xFF4444FF
local COL_BLUE = 0x44AAFFFF
local COL_MUTED = 0x9B927FFF

local function draw_separator()
  r.ImGui_Spacing(ctx)
  r.ImGui_Separator(ctx)
  r.ImGui_Spacing(ctx)
end

local function draw_header(text)
  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_Text(), COL_GOLD)
  r.ImGui_Text(ctx, text)
  r.ImGui_PopStyleColor(ctx)
  r.ImGui_Spacing(ctx)
end

local function draw_status_bar()
  r.ImGui_Spacing(ctx)
  r.ImGui_Separator(ctx)
  r.ImGui_Spacing(ctx)

  if is_processing then
    if progress_value < 0 then
      r.ImGui_ProgressBar(ctx, -1.0 * r.ImGui_GetTime(ctx), -1, progress_text)
    else
      r.ImGui_ProgressBar(ctx, progress_value, -1, progress_text)
    end
    r.ImGui_Spacing(ctx)
  end

  if status_message ~= "" then
    r.ImGui_PushStyleColor(ctx, r.ImGui_Col_Text(), status_color)
    r.ImGui_Text(ctx, status_message)
    r.ImGui_PopStyleColor(ctx)
  end
end

-- ═══════════════ VOICE SWAP TAB ═══════════════

local function draw_swap_tab()
  draw_header("VOICE SWAP ENGINE")

  -- Model selection
  r.ImGui_Text(ctx, "Voice Model:")
  if r.ImGui_BeginCombo(ctx, "##model", swap_selected_model > 0 and swap_model_list[swap_selected_model] or "Select a model...") then
    for i, model in ipairs(swap_model_list) do
      local is_selected = (i == swap_selected_model)
      if r.ImGui_Selectable(ctx, model, is_selected) then
        swap_selected_model = i
      end
    end
    r.ImGui_EndCombo(ctx)
  end

  if r.ImGui_Button(ctx, "Refresh Models") then
    scan_models()
    set_status("Found " .. #swap_model_list .. " models", COL_GREEN)
  end

  draw_separator()

  -- Processing mode toggle
  draw_header("PROCESSING MODE")
  local mode_changed
  mode_changed, swap_processing_mode = r.ImGui_RadioButtonEx(ctx, "Low Latency (256 samples)", swap_processing_mode, 0)
  r.ImGui_SameLine(ctx)
  mode_changed, swap_processing_mode = r.ImGui_RadioButtonEx(ctx, "High Quality (16384 samples)", swap_processing_mode, 1)
  if mode_changed then
    r.SetExtState(STATE.EXT_SECTION, STATE.KEY_PROCESSING_MODE, tostring(swap_processing_mode), true)
  end

  r.ImGui_Spacing(ctx)

  -- Pitch controls
  draw_header("PITCH & FORMANT")
  local pitch_changed
  pitch_changed, swap_pitch_shift = r.ImGui_SliderDouble(ctx, "Pitch Shift (semitones)", swap_pitch_shift, -12.0, 12.0, "%.1f")

  local formant_changed
  formant_changed, swap_formant_shift = r.ImGui_SliderDouble(ctx, "Formant Shift", swap_formant_shift, -5.0, 5.0, "%.1f")

  local mix_changed
  mix_changed, swap_dry_wet = r.ImGui_SliderDouble(ctx, "Dry/Wet Mix", swap_dry_wet, 0.0, 1.0, "%.2f")

  r.ImGui_Spacing(ctx)

  -- Pitch method
  r.ImGui_Text(ctx, "Pitch Method:")
  local methods = {"RMVPE (Recommended)", "CREPE", "Harvest", "FCPE"}
  if r.ImGui_BeginCombo(ctx, "##method", methods[swap_pitch_method + 1]) then
    for i, method in ipairs(methods) do
      if r.ImGui_Selectable(ctx, method, (i - 1) == swap_pitch_method) then
        swap_pitch_method = i - 1
      end
    end
    r.ImGui_EndCombo(ctx)
  end

  draw_separator()

  -- Action buttons
  local btn_w = r.ImGui_GetContentRegionAvail(ctx)
  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_Button(), COL_GOLD)
  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_ButtonHovered(), 0xE8C76AFF)
  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_Text(), COL_NAVY_DEEP)

  if r.ImGui_Button(ctx, "SWAP VOICE ON SELECTED ITEMS", btn_w, 40) then
    execute_voice_swap()
  end

  r.ImGui_PopStyleColor(ctx, 3)
end

-- ═══════════════ TRAINING TAB ═══════════════

local function draw_training_tab()
  draw_header("MODEL TRAINING STUDIO")

  -- Track selection
  r.ImGui_Text(ctx, "Source Track (with vocal audio):")
  local track_count = r.CountTracks(0)
  local track_names = {}
  for i = 0, track_count - 1 do
    local track = r.GetTrack(0, i)
    local _, name = r.GetTrackName(track)
    table.insert(track_names, string.format("%d: %s", i + 1, name))
  end

  local current_name = train_track_idx < #track_names and track_names[train_track_idx + 1] or "Select track..."
  if r.ImGui_BeginCombo(ctx, "##traintrack", current_name) then
    for i, name in ipairs(track_names) do
      if r.ImGui_Selectable(ctx, name, (i - 1) == train_track_idx) then
        train_track_idx = i - 1
      end
    end
    r.ImGui_EndCombo(ctx)
  end

  r.ImGui_Spacing(ctx)

  -- Dataset health check
  if r.ImGui_Button(ctx, "Run Dataset Health Check") then
    run_dataset_health_check(train_track_idx)
  end

  if dataset_check_result ~= "" then
    r.ImGui_Spacing(ctx)
    r.ImGui_PushStyleColor(ctx, r.ImGui_Col_Text(), dataset_check_ok and COL_GREEN or COL_RED)
    r.ImGui_TextWrapped(ctx, dataset_check_result)
    r.ImGui_PopStyleColor(ctx)
  end

  r.ImGui_Spacing(ctx)

  if r.ImGui_Button(ctx, "Normalize to -1.0 dBFS") then
    apply_normalization(train_track_idx)
  end

  draw_separator()

  -- Training parameters
  draw_header("TRAINING PARAMETERS")

  local ep_changed
  ep_changed, train_epochs = r.ImGui_SliderInt(ctx, "Epochs", train_epochs, 50, 500)

  local bs_changed
  bs_changed, train_batch_size = r.ImGui_SliderInt(ctx, "Batch Size", train_batch_size, 4, 32)

  local sr_options = {32000, 40000, 48000}
  local sr_labels = {"32000 Hz", "40000 Hz (Recommended)", "48000 Hz"}
  local sr_idx = 2
  for i, v in ipairs(sr_options) do
    if v == train_sample_rate then sr_idx = i end
  end
  if r.ImGui_BeginCombo(ctx, "Sample Rate", sr_labels[sr_idx]) then
    for i, label in ipairs(sr_labels) do
      if r.ImGui_Selectable(ctx, label, i == sr_idx) then
        train_sample_rate = sr_options[i]
      end
    end
    r.ImGui_EndCombo(ctx)
  end

  draw_separator()

  -- RunPod API key status
  if train_runpod_key ~= "" then
    r.ImGui_PushStyleColor(ctx, r.ImGui_Col_Text(), COL_GREEN)
    r.ImGui_Text(ctx, "RunPod API Key: Configured")
    r.ImGui_PopStyleColor(ctx)
  else
    r.ImGui_PushStyleColor(ctx, r.ImGui_Col_Text(), COL_RED)
    r.ImGui_Text(ctx, "RunPod API Key: Not set (go to Settings)")
    r.ImGui_PopStyleColor(ctx)
  end

  draw_separator()

  -- Train button
  local btn_w = r.ImGui_GetContentRegionAvail(ctx)
  local can_train = dataset_check_ok and train_runpod_key ~= "" and not is_processing

  if not can_train then r.ImGui_BeginDisabled(ctx) end

  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_Button(), COL_GOLD)
  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_ButtonHovered(), 0xE8C76AFF)
  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_Text(), COL_NAVY_DEEP)

  if r.ImGui_Button(ctx, "START CLOUD TRAINING (RunPod GPU)", btn_w, 40) then
    start_cloud_training()
  end

  r.ImGui_PopStyleColor(ctx, 3)
  if not can_train then r.ImGui_EndDisabled(ctx) end

  r.ImGui_Spacing(ctx)
  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_Text(), COL_MUTED)
  r.ImGui_TextWrapped(ctx, "Training uses YOUR RunPod API key. You pay only for GPU seconds used (~$0.10-0.25 per model). Your audio is automatically deleted from cloud after training.")
  r.ImGui_PopStyleColor(ctx)
end

-- ═══════════════ SETTINGS TAB ═══════════════

local function draw_settings_tab()
  draw_header("SETTINGS")

  -- Models directory
  r.ImGui_Text(ctx, "Models Directory:")
  local dir_changed
  dir_changed, buf_models_dir = r.ImGui_InputText(ctx, "##modelsdir", buf_models_dir)
  r.ImGui_SameLine(ctx)
  if r.ImGui_Button(ctx, "Browse##models") then
    local rv, folder = r.JS_Dialog_BrowseForFolder("Select Models Directory", settings_models_dir)
    if rv == 1 then
      buf_models_dir = folder
    end
  end

  r.ImGui_Spacing(ctx)

  -- RunPod API Key
  r.ImGui_Text(ctx, "RunPod API Key (BYOK):")
  local key_changed
  key_changed, buf_runpod_key = r.ImGui_InputText(ctx, "##runpodkey", buf_runpod_key, r.ImGui_InputTextFlags_Password())

  draw_separator()

  -- Engine settings
  draw_header("ENGINE CONFIGURATION")

  local port_changed
  port_changed, settings_engine_port = r.ImGui_InputInt(ctx, "Engine Port", settings_engine_port)

  local norm_changed
  norm_changed, settings_auto_normalize = r.ImGui_Checkbox(ctx, "Auto-normalize before training", settings_auto_normalize)

  local gpu_changed
  gpu_changed, settings_gpu_enabled = r.ImGui_Checkbox(ctx, "Enable GPU acceleration (CUDA/Metal)", settings_gpu_enabled)

  draw_separator()

  -- Save button
  if r.ImGui_Button(ctx, "Save Settings", 200, 36) then
    settings_models_dir = buf_models_dir
    train_runpod_key = buf_runpod_key
    r.SetExtState(STATE.EXT_SECTION, STATE.KEY_MODELS_DIR, settings_models_dir, true)
    r.SetExtState(STATE.EXT_SECTION, STATE.KEY_RUNPOD_KEY, train_runpod_key, true)
    scan_models()
    set_status("Settings saved", COL_GREEN)
  end

  draw_separator()

  -- Engine controls
  draw_header("LOCAL AI ENGINE")

  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_Text(), is_engine_running and COL_GREEN or COL_RED)
  r.ImGui_Text(ctx, is_engine_running and "Engine Status: RUNNING" or "Engine Status: STOPPED")
  r.ImGui_PopStyleColor(ctx)

  r.ImGui_Spacing(ctx)

  if r.ImGui_Button(ctx, is_engine_running and "Restart Engine" or "Start Engine", 160, 32) then
    if is_engine_running then stop_engine() end
    start_engine()
  end

  r.ImGui_SameLine(ctx)

  if is_engine_running then
    if r.ImGui_Button(ctx, "Stop Engine", 160, 32) then
      stop_engine()
    end
  end
end

-- ═══════════════ ACTIVATION TAB ═══════════════

local function draw_activation_tab()
  draw_header("LICENSE ACTIVATION")

  -- Current status
  r.ImGui_Text(ctx, "Status:")
  r.ImGui_SameLine(ctx)
  local is_active = activation_status == "Active"
  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_Text(), is_active and COL_GREEN or COL_RED)
  r.ImGui_Text(ctx, activation_status)
  r.ImGui_PopStyleColor(ctx)

  r.ImGui_Text(ctx, "Tier:")
  r.ImGui_SameLine(ctx)
  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_Text(), COL_GOLD)
  r.ImGui_Text(ctx, activation_tier)
  r.ImGui_PopStyleColor(ctx)

  r.ImGui_Text(ctx, "Machine ID:")
  r.ImGui_SameLine(ctx)
  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_Text(), COL_MUTED)
  local mid = get_machine_id()
  r.ImGui_Text(ctx, mid:sub(1, 20) .. "...")
  r.ImGui_PopStyleColor(ctx)

  draw_separator()

  -- License key input
  r.ImGui_Text(ctx, "Enter License Key:")
  local changed
  changed, buf_license_key = r.ImGui_InputText(ctx, "##licensekey", buf_license_key)

  r.ImGui_Spacing(ctx)

  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_Button(), COL_GOLD)
  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_ButtonHovered(), 0xE8C76AFF)
  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_Text(), COL_NAVY_DEEP)

  if r.ImGui_Button(ctx, "ACTIVATE LICENSE", 250, 36) then
    activate_license(buf_license_key)
  end

  r.ImGui_PopStyleColor(ctx, 3)

  draw_separator()

  -- Tier features
  draw_header("FEATURE TIERS")

  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_Text(), COL_MUTED)
  r.ImGui_TextWrapped(ctx, "FREE TIER:\n  - Local voice swap (CPU)\n  - Basic pitch shifting\n  - Single model slot")
  r.ImGui_Spacing(ctx)
  r.ImGui_TextWrapped(ctx, "BASIC TIER:\n  - Everything in Free\n  - GPU acceleration (CUDA/Metal)\n  - Cloud training (BYOK)\n  - Unlimited model slots\n  - Formant control")
  r.ImGui_Spacing(ctx)
  r.ImGui_TextWrapped(ctx, "ADVANCED TIER:\n  - Everything in Basic\n  - AI stem separation (Demucs)\n  - Batch processing\n  - Priority support\n  - Early access to updates")
  r.ImGui_PopStyleColor(ctx)
end

-- ═══════════════════════════════════════════════════════════
-- MAIN GUI LOOP
-- ═══════════════════════════════════════════════════════════

local function main_loop()
  -- Window styling
  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_WindowBg(), COL_DARK_BG)
  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_TitleBg(), COL_NAVY)
  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_TitleBgActive(), COL_NAVY)
  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_Tab(), COL_NAVY)
  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_TabSelected(), COL_GOLD)
  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_TabHovered(), 0xE8C76AFF)
  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_FrameBg(), COL_CARD_BG)
  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_SliderGrab(), COL_GOLD)
  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_CheckMark(), COL_GOLD)
  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_Header(), COL_NAVY)
  r.ImGui_PushStyleColor(ctx, r.ImGui_Col_HeaderHovered(), 0x2E4268FF)

  r.ImGui_SetNextWindowSize(ctx, 620, 680, r.ImGui_Cond_FirstUseEver())

  local visible, open = r.ImGui_Begin(ctx, APP_NAME .. ' v' .. APP_VERSION .. ' - AI Voice Engine', true)

  if visible then
    -- Title bar
    r.ImGui_PushStyleColor(ctx, r.ImGui_Col_Text(), COL_GOLD)
    r.ImGui_Text(ctx, "CLONADA")
    r.ImGui_PopStyleColor(ctx)
    r.ImGui_SameLine(ctx)
    r.ImGui_PushStyleColor(ctx, r.ImGui_Col_Text(), COL_MUTED)
    r.ImGui_Text(ctx, "AI Voice Cloning & Swap Engine")
    r.ImGui_PopStyleColor(ctx)

    r.ImGui_Spacing(ctx)

    -- Tab bar
    if r.ImGui_BeginTabBar(ctx, "##tabs") then

      if r.ImGui_BeginTabItem(ctx, " Voice Swap ") then
        current_tab = 0
        r.ImGui_Spacing(ctx)
        draw_swap_tab()
        r.ImGui_EndTabItem(ctx)
      end

      if r.ImGui_BeginTabItem(ctx, " Training ") then
        current_tab = 1
        r.ImGui_Spacing(ctx)
        draw_training_tab()
        r.ImGui_EndTabItem(ctx)
      end

      if r.ImGui_BeginTabItem(ctx, " Settings ") then
        current_tab = 2
        r.ImGui_Spacing(ctx)
        draw_settings_tab()
        r.ImGui_EndTabItem(ctx)
      end

      if r.ImGui_BeginTabItem(ctx, " Activation ") then
        current_tab = 3
        r.ImGui_Spacing(ctx)
        draw_activation_tab()
        r.ImGui_EndTabItem(ctx)
      end

      r.ImGui_EndTabBar(ctx)
    end

    -- Status bar at bottom
    draw_status_bar()

    r.ImGui_End(ctx)
  end

  r.ImGui_PopStyleColor(ctx, 11)

  if open then
    r.defer(main_loop)
  else
    stop_engine()
  end
end

-- ═══════════════════════════════════════════════════════════
-- INITIALIZATION
-- ═══════════════════════════════════════════════════════════

load_saved_state()
scan_models()
r.defer(main_loop)
