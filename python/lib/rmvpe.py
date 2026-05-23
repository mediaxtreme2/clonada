"""
RMVPE - Robust Model for Vocal Pitch Estimation
Deep ResUNet + BiGRU + Linear(360) for pitch class prediction.
Architecture matches the official RMVPE pretrained weights.
"""

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F


class ResBlock(nn.Module):
    def __init__(self, in_ch, out_ch):
        super().__init__()
        self.conv = nn.Sequential(
            nn.Conv2d(in_ch, out_ch, 3, 1, 1, bias=False),
            nn.BatchNorm2d(out_ch),
            nn.ReLU(inplace=True),
            nn.Conv2d(out_ch, out_ch, 3, 1, 1, bias=False),
            nn.BatchNorm2d(out_ch),
        )
        self.shortcut = nn.Conv2d(in_ch, out_ch, 1) if in_ch != out_ch else None

    def forward(self, x):
        identity = self.shortcut(x) if self.shortcut is not None else x
        return F.relu(self.conv(x) + identity)


class ResLayer(nn.Module):
    def __init__(self, in_ch, out_ch, n_blocks=4):
        super().__init__()
        self.conv = nn.ModuleList([ResBlock(in_ch, out_ch)])
        for _ in range(n_blocks - 1):
            self.conv.append(ResBlock(out_ch, out_ch))

    def forward(self, x):
        for block in self.conv:
            x = block(x)
        return x


class UNetEncoder(nn.Module):
    def __init__(self):
        super().__init__()
        self.bn = nn.BatchNorm2d(1)
        channels = [1, 16, 32, 64, 128, 256]
        self.layers = nn.ModuleList()
        for i in range(5):
            self.layers.append(ResLayer(channels[i], channels[i + 1]))
        self.pool = nn.MaxPool2d(2)

    def forward(self, x):
        x = self.bn(x)
        skips = []
        for layer in self.layers:
            x = layer(x)
            skips.append(x)
            x = self.pool(x)
        return x, skips


class UNetDecoder(nn.Module):
    def __init__(self):
        super().__init__()
        enc_channels = [256, 128, 64, 32, 16]
        dec_in = [512, 256, 128, 64, 32]
        dec_out = [256, 128, 64, 32, 16]

        self.layers = nn.ModuleList()
        for i in range(5):
            self.layers.append(DecoderLayer(dec_in[i], dec_out[i], enc_channels[i]))

    def forward(self, x, skips):
        for i, layer in enumerate(self.layers):
            x = layer(x, skips[-(i + 1)])
        return x


class DecoderLayer(nn.Module):
    def __init__(self, in_ch, out_ch, skip_ch):
        super().__init__()
        self.conv1 = nn.Sequential(
            nn.ConvTranspose2d(in_ch, out_ch, 3, stride=2, padding=1, output_padding=1, bias=False),
            nn.BatchNorm2d(out_ch),
        )
        concat_ch = out_ch + skip_ch
        self.conv2 = nn.ModuleList([ResBlock(concat_ch, out_ch)])
        for _ in range(3):
            self.conv2.append(ResBlock(out_ch, out_ch))

    def forward(self, x, skip):
        x = self.conv1(x)
        if x.shape[2:] != skip.shape[2:]:
            x = F.interpolate(x, size=skip.shape[2:], mode='bilinear', align_corners=False)
        x = torch.cat([x, skip], dim=1)
        for block in self.conv2:
            x = block(x)
        return x


class BiGRUWrapper(nn.Module):
    def __init__(self, input_size, hidden_size):
        super().__init__()
        self.gru = nn.GRU(input_size, hidden_size, batch_first=True, bidirectional=True)

    def forward(self, x):
        return self.gru(x)[0]


class Intermediate(nn.Module):
    def __init__(self):
        super().__init__()
        self.layers = nn.ModuleList([
            ResLayer(256, 512, n_blocks=4),
            ResLayer(512, 512, n_blocks=4),
            ResLayer(512, 512, n_blocks=4),
            ResLayer(512, 512, n_blocks=4),
        ])

    def forward(self, x):
        for layer in self.layers:
            x = layer(x)
        return x


class DeepUnet(nn.Module):
    def __init__(self):
        super().__init__()
        self.encoder = UNetEncoder()
        self.intermediate = Intermediate()
        self.decoder = UNetDecoder()

    def forward(self, x):
        x, skips = self.encoder(x)
        x = self.intermediate(x)
        x = self.decoder(x, skips)
        return x


class E2E(nn.Module):
    """End-to-end RMVPE: DeepUnet -> Conv -> BiGRU -> Linear(360)."""

    def __init__(self):
        super().__init__()
        self.unet = DeepUnet()
        self.cnn = nn.Conv2d(16, 3, 3, padding=1)
        self.fc = nn.Sequential(
            BiGRUWrapper(384, 256),
            nn.Linear(512, 360),
        )

    def forward(self, mel):
        x = self.unet(mel)
        x = self.cnn(x)
        b, c, f, t = x.shape
        x = x.permute(0, 3, 1, 2).reshape(b, t, c * f)
        x = self.fc(x)
        x = torch.sigmoid(x)
        return x


class MelSpectrogram(nn.Module):
    def __init__(self, n_mel=128, sr=16000, n_fft=1024, hop=160):
        super().__init__()
        self.n_mel = n_mel
        self.sr = sr
        self.n_fft = n_fft
        self.hop = hop

        import torchaudio
        self.mel_transform = torchaudio.transforms.MelSpectrogram(
            sample_rate=sr, n_fft=n_fft, hop_length=hop, n_mels=n_mel,
            f_min=30, f_max=sr // 2, power=1.0
        )

    def forward(self, audio):
        mel = self.mel_transform(audio)
        mel = torch.log(torch.clamp(mel, min=1e-5))
        return mel


class RMVPE:
    """RMVPE pitch extraction wrapper."""

    CENTS_MAPPING = np.arange(360) * 20 + 1997.3794084376191

    def __init__(self, model_path, is_half=False, device="cpu"):
        self.device = device
        self.is_half = is_half
        self.mel_extractor = MelSpectrogram().to(device)

        self.model = E2E()
        ckpt = torch.load(model_path, map_location="cpu", weights_only=False)
        if isinstance(ckpt, dict) and "model" in ckpt:
            self.model.load_state_dict(ckpt["model"], strict=False)
        else:
            self.model.load_state_dict(ckpt, strict=False)

        self.model = self.model.to(device)
        self.model.eval()

        if is_half:
            self.model = self.model.half()

    def infer_from_audio(self, audio, thred=0.03):
        if audio.dim() == 1:
            audio = audio.unsqueeze(0)

        audio = audio.to(self.device)

        with torch.no_grad():
            mel = self.mel_extractor(audio)
            mel = mel.unsqueeze(1)

            if self.is_half:
                mel = mel.half()

            output = self.model(mel)

        output = output[0].cpu().numpy()
        return self._decode_pitch(output, thred)

    def _decode_pitch(self, output, thred):
        confidence = np.max(output, axis=1)
        f0 = np.zeros(len(output))

        for i in range(len(output)):
            if confidence[i] < thred:
                continue

            probs = output[i]
            center = np.argmax(probs)

            start = max(0, center - 2)
            end = min(360, center + 3)
            weights = probs[start:end]
            cents = self.CENTS_MAPPING[start:end]

            if np.sum(weights) > 0:
                avg_cents = np.sum(weights * cents) / np.sum(weights)
                f0[i] = 10 * (2 ** (avg_cents / 1200))

        return f0
