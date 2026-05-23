"""
RMVPE - Robust Model for Vocal Pitch Estimation
DeepUnet encoder-decoder + BiGRU + Linear(360) for pitch class prediction.
"""

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F


class ConvBlock(nn.Module):
    def __init__(self, in_ch, out_ch, kernel=3, stride=1):
        super().__init__()
        self.conv = nn.Conv2d(in_ch, out_ch, kernel, stride, kernel // 2)
        self.bn = nn.BatchNorm2d(out_ch)

    def forward(self, x):
        return F.relu(self.bn(self.conv(x)))


class EncoderBlock(nn.Module):
    def __init__(self, in_ch, out_ch):
        super().__init__()
        self.conv1 = ConvBlock(in_ch, out_ch)
        self.conv2 = ConvBlock(out_ch, out_ch)
        self.pool = nn.MaxPool2d(2)

    def forward(self, x):
        x = self.conv1(x)
        x = self.conv2(x)
        return self.pool(x), x  # pooled, skip


class DecoderBlock(nn.Module):
    def __init__(self, in_ch, out_ch):
        super().__init__()
        self.up = nn.ConvTranspose2d(in_ch, out_ch, 2, 2)
        self.conv1 = ConvBlock(out_ch * 2, out_ch)
        self.conv2 = ConvBlock(out_ch, out_ch)

    def forward(self, x, skip):
        x = self.up(x)
        # Handle size mismatch
        if x.shape != skip.shape:
            x = F.interpolate(x, size=skip.shape[2:])
        x = torch.cat([x, skip], dim=1)
        x = self.conv1(x)
        return self.conv2(x)


class DeepUnet(nn.Module):
    """5-level UNet for pitch estimation."""

    def __init__(self):
        super().__init__()
        channels = [1, 16, 32, 64, 128, 256]

        self.encoders = nn.ModuleList()
        for i in range(5):
            self.encoders.append(EncoderBlock(channels[i], channels[i + 1]))

        self.bottleneck = nn.Sequential(
            ConvBlock(256, 512),
            ConvBlock(512, 256)
        )

        self.decoders = nn.ModuleList()
        for i in range(4, -1, -1):
            self.decoders.append(DecoderBlock(channels[i + 1], channels[i]))

    def forward(self, x):
        skips = []
        for enc in self.encoders:
            x, skip = enc(x)
            skips.append(skip)

        x = self.bottleneck(x)

        for i, dec in enumerate(self.decoders):
            x = dec(x, skips[-(i + 1)])

        return x


class E2E(nn.Module):
    """End-to-end RMVPE model: DeepUnet -> CNN -> BiGRU -> Linear(360)."""

    def __init__(self):
        super().__init__()
        self.unet = DeepUnet()
        self.cnn = nn.Sequential(
            nn.Conv2d(1, 3, (3, 3), padding=(1, 1)),
            nn.BatchNorm2d(3),
            nn.ReLU()
        )
        self.fc = nn.Sequential(
            nn.Linear(3 * 128, 256),
            nn.ReLU(),
            nn.Dropout(0.25)
        )
        self.gru = nn.GRU(256, 128, batch_first=True, bidirectional=True)
        self.out = nn.Linear(256, 360)

    def forward(self, mel):
        # mel: [B, 1, 128, T]
        x = self.unet(mel)  # [B, 1, 128, T]
        x = self.cnn(x)  # [B, 3, 128, T]

        # Reshape for GRU: [B, T, 3*128]
        b, c, f, t = x.shape
        x = x.permute(0, 3, 1, 2).reshape(b, t, c * f)
        x = self.fc(x)
        x, _ = self.gru(x)  # [B, T, 256]
        x = torch.sigmoid(self.out(x))  # [B, T, 360]
        return x


class MelSpectrogram(nn.Module):
    """Compute mel spectrogram for RMVPE input."""

    def __init__(self, n_mel=128, sr=16000, n_fft=1024, hop=160):
        super().__init__()
        self.n_mel = n_mel
        self.sr = sr
        self.n_fft = n_fft
        self.hop = hop

        import torchaudio
        self.mel_transform = torchaudio.transforms.MelSpectrogram(
            sample_rate=sr,
            n_fft=n_fft,
            hop_length=hop,
            n_mels=n_mel,
            f_min=30,
            f_max=sr // 2,
            power=1.0
        )

    def forward(self, audio):
        mel = self.mel_transform(audio)
        mel = torch.log(torch.clamp(mel, min=1e-5))
        return mel


class RMVPE:
    """RMVPE pitch extraction wrapper."""

    # 360 pitch classes: cents mapping from ~C1 to ~B6
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
        """
        Extract f0 from audio tensor.

        Args:
            audio: torch tensor of audio at 16kHz
            thred: voicing threshold (0-1)

        Returns:
            numpy array of f0 values in Hz, 0 for unvoiced
        """
        if audio.dim() == 1:
            audio = audio.unsqueeze(0)

        audio = audio.to(self.device)

        with torch.no_grad():
            mel = self.mel_extractor(audio)  # [1, 128, T]
            mel = mel.unsqueeze(1)  # [1, 1, 128, T]

            if self.is_half:
                mel = mel.half()

            output = self.model(mel)  # [1, T, 360]

        output = output[0].cpu().numpy()  # [T, 360]
        return self._decode_pitch(output, thred)

    def _decode_pitch(self, output, thred):
        """Decode 360-class output to Hz values."""
        # output: [T, 360]
        confidence = np.max(output, axis=1)
        f0 = np.zeros(len(output))

        for i in range(len(output)):
            if confidence[i] < thred:
                f0[i] = 0.0
                continue

            # Weighted average of top activations for sub-bin precision
            probs = output[i]
            center = np.argmax(probs)

            # Use 5-bin window around peak
            start = max(0, center - 2)
            end = min(360, center + 3)
            weights = probs[start:end]
            cents = self.CENTS_MAPPING[start:end]

            if np.sum(weights) > 0:
                avg_cents = np.sum(weights * cents) / np.sum(weights)
                f0[i] = 10 * (2 ** (avg_cents / 1200))
            else:
                f0[i] = 0.0

        return f0
