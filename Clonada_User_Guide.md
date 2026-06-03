# CLONADA AI VOCAL SUITE - Producer Quick Start Guide

## INSTALLATION (macOS)

1. Download the latest Clonada DMG from the link provided
2. Double-click to open the DMG file
3. Inside you will see a file called "Install Clonada.command"
4. Double-click "Install Clonada.command"
5. If macOS says "cannot be opened because it is from an unidentified developer":
   - Right-click (or Control+click) the file
   - Select "Open" from the menu
   - Click "Open" in the dialog that appears
6. A Terminal window will open showing the installation progress
7. Wait for it to complete (5-15 minutes on first install - it downloads AI models)
8. When you see "INSTALLATION COMPLETE!" you can close the Terminal window
9. Open your DAW (REAPER, Logic Pro, etc.) and rescan plugins
10. Clonada will appear in your plugin list (VST3, Audio Unit, or CLAP)


## ACTIVATING YOUR LICENSE

1. Load Clonada on any audio track in your DAW
2. The plugin window will open showing the activation screen
3. Enter your license key (format: CLON-XXXX-XXXX-XXXX-XXXX)
4. Click "Activate"
5. You should see "License Active" with your tier level (Basic or Advanced)


## VOICE CLONING (Training a New Voice Model)

This creates a custom voice model from a sample audio file.

1. Open Clonada in your DAW
2. Click the "MODEL BROWSER" tab at the top
3. Click the "TRAIN" button (amber/orange colored)
4. Select a clean vocal WAV or MP3 file (at least 30 seconds of singing/speaking)
   - Use clean vocals with no background music
   - Longer samples (1-3 minutes) produce better results
   - Multiple files can be selected
5. A dialog will confirm "Training Submitted"
6. Training happens on cloud GPU and takes 5-15 minutes
7. When complete, the model appears in your Models list automatically
8. Give the model a descriptive name (e.g., "Deep Male Vocal", "Female Soul Singer")

TIPS FOR BETTER TRAINING:
- Use high-quality recordings (at least 44.1kHz, preferably WAV)
- Remove silence, noise, and spoken sections - only include singing
- More variety in the sample (different notes, dynamics) = better model
- Minimum 30 seconds, recommended 2-5 minutes of clean vocal material


## VOICE SWAPPING (Converting Audio)

This converts any vocal recording to sound like the trained voice model.

1. Record or import a vocal track in your DAW
2. Load Clonada as an insert effect on that vocal track
3. Make sure your license is activated
4. In the MODEL BROWSER, click on the voice model you want to use
5. Click "LOAD" to select it as the active model
6. The model name will appear at the top of the plugin

REAL-TIME CONVERSION:
7. Press Play in your DAW - you'll hear the converted voice in real-time
8. Adjust these parameters to fine-tune:
   - PITCH SHIFT: Shift the voice up/down in semitones (-12 to +12)
   - MIX: Blend between original (0%) and converted (100%) voice
   - INDEX RATE: How closely to match the target voice (0.0 to 1.0)
     Higher = more like the model, lower = more natural variation

OFFLINE/BOUNCE CONVERSION (Higher Quality):
9. For final renders, bounce/export the track through Clonada
10. The offline render uses higher quality processing


## STEM SEPARATION

Split any song into separate stems (vocals, drums, bass, other).

1. Load Clonada on the track containing the full mix
2. Go to the "TOOLS" section
3. Click "SEPARATE STEMS"
4. Select the audio file you want to split
5. Choose the output folder
6. Processing takes 1-3 minutes per song
7. Four files will be created:
   - vocals.wav - Isolated vocals
   - drums.wav - Drums and percussion
   - bass.wav - Bass instruments
   - other.wav - Everything else (guitars, keys, synths)


## PRESETS

Save and load your favorite parameter combinations.

SAVING A PRESET:
1. Set up your preferred Pitch, Mix, Index Rate, and other parameters
2. Click "SAVE PRESET" in the preset section
3. Give it a name (e.g., "Male to Female +4", "Subtle Blend 50%")

LOADING A PRESET:
1. Click the preset dropdown
2. Select from your saved presets or factory presets
3. All parameters will be applied instantly


## MANAGING VOICE MODELS

VIEW MODELS:
- Click the MODEL BROWSER tab to see all installed models
- Models are stored in ~/Clonada/models/ on your Mac

IMPORT A MODEL:
- Click "IMPORT" in the Model Browser
- Select a .pth model file
- It will be copied to your models folder

DELETE A MODEL:
- Find the model file in ~/Clonada/models/
- Delete the .pth file (and .index file if present)
- Refresh the model list in Clonada


## TROUBLESHOOTING

ENGINE OFFLINE:
- This means the AI engine is not running
- Close your DAW completely
- Open Terminal and run: bash ~/Clonada/start_engine.sh
- Wait for "Listening on tcp://127.0.0.1:5050" to appear
- Reopen your DAW and load Clonada
- If this persists, reinstall using the DMG installer

NO MODELS SHOWING:
- Make sure you have trained at least one model first
- Check that models exist in ~/Clonada/models/
- Models are .pth files

CRACKLING/GLITCHES:
- Increase your DAW buffer size (try 512 or 1024 samples)
- Close other CPU-heavy applications
- Make sure you're not running too many instances of Clonada simultaneously

PLUGIN NOT APPEARING IN DAW:
- Make sure you ran the installer (not just copied the file)
- In your DAW, do a full plugin rescan
- Check these locations for the plugin:
  - VST3: ~/Library/Audio/Plug-Ins/VST3/Clonada.vst3
  - AU: ~/Library/Audio/Plug-Ins/Components/Clonada.component
  - CLAP: ~/Library/Audio/Plug-Ins/CLAP/Clonada.clap


## SYSTEM REQUIREMENTS

- macOS 12 or later (Apple Silicon native support)
- 8 GB RAM minimum (16 GB recommended)
- Internet connection for license activation and cloud training
- Compatible DAW: REAPER, Logic Pro, Ableton Live, FL Studio, Cubase, Pro Tools, Bitwig, GarageBand, or any VST3/AU/CLAP host


## SUPPORT

For technical support, contact your Clonada administrator.
License key issues: Check that your key is entered exactly as provided (including dashes).
