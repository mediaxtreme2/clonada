"""
RVC v2 Neural Network Architecture
SynthesizerTrnMs768NSFsid - Voice conversion network with NSF-based decoder.
"""

import math
import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.nn.utils import weight_norm, remove_weight_norm


class TextEncoder768(nn.Module):
    """Encodes HuBERT features (768-dim) + pitch into latent representation."""

    def __init__(self, out_channels, hidden_channels, filter_channels,
                 n_heads, n_layers, kernel_size, p_dropout, f0=True):
        super().__init__()
        self.out_channels = out_channels
        self.hidden_channels = hidden_channels
        self.f0 = f0

        self.emb_phone = nn.Linear(768, hidden_channels)
        if f0:
            self.emb_pitch = nn.Embedding(256, hidden_channels)

        self.encoder = Encoder(hidden_channels, filter_channels, n_heads,
                              n_layers, kernel_size, p_dropout)
        self.proj = nn.Conv1d(hidden_channels, out_channels * 2, 1)

    def forward(self, phone, pitch, lengths):
        x = self.emb_phone(phone)
        if self.f0 and pitch is not None:
            x = x + self.emb_pitch(pitch)

        x = x.transpose(1, 2)  # [B, C, T]
        x_mask = torch.ones(x.shape[0], 1, x.shape[2], device=x.device)

        if lengths is not None:
            for i, l in enumerate(lengths):
                x_mask[i, :, l:] = 0

        x = self.encoder(x * x_mask, x_mask)
        stats = self.proj(x) * x_mask
        m, logs = torch.split(stats, self.out_channels, dim=1)
        return m, logs, x_mask


class LayerNorm(nn.Module):
    """LayerNorm with gamma/beta parameter names (RVC checkpoint format)."""

    def __init__(self, channels, eps=1e-5):
        super().__init__()
        self.channels = channels
        self.eps = eps
        self.gamma = nn.Parameter(torch.ones(channels))
        self.beta = nn.Parameter(torch.zeros(channels))

    def forward(self, x):
        x = x.transpose(1, -1)
        x = F.layer_norm(x, (self.channels,), self.gamma, self.beta, self.eps)
        return x.transpose(1, -1)


class Encoder(nn.Module):
    """Multi-head attention encoder with FFN layers."""

    def __init__(self, hidden_channels, filter_channels, n_heads,
                 n_layers, kernel_size, p_dropout, window_size=10):
        super().__init__()
        self.hidden_channels = hidden_channels
        self.n_layers = n_layers

        self.attn_layers = nn.ModuleList()
        self.norm_layers_1 = nn.ModuleList()
        self.norm_layers_2 = nn.ModuleList()
        self.ffn_layers = nn.ModuleList()

        for _ in range(n_layers):
            self.attn_layers.append(
                MultiHeadAttention(hidden_channels, hidden_channels, n_heads,
                                   p_dropout, window_size=window_size)
            )
            self.norm_layers_1.append(LayerNorm(hidden_channels))
            self.norm_layers_2.append(LayerNorm(hidden_channels))
            self.ffn_layers.append(
                FFN(hidden_channels, filter_channels, kernel_size, p_dropout)
            )

        self.drop = nn.Dropout(p_dropout)

    def forward(self, x, x_mask):
        attn_mask = x_mask.unsqueeze(2) * x_mask.unsqueeze(-1)
        x = x * x_mask
        for i in range(self.n_layers):
            y = self.attn_layers[i](x, x, attn_mask)
            y = self.drop(y)
            x = self.norm_layers_1[i](x + y)
            y = self.ffn_layers[i](x, x_mask)
            y = self.drop(y)
            x = self.norm_layers_2[i](x + y)
        x = x * x_mask
        return x


class MultiHeadAttention(nn.Module):
    def __init__(self, channels, out_channels, n_heads, p_dropout=0.0,
                 window_size=None):
        super().__init__()
        self.channels = channels
        self.out_channels = out_channels
        self.n_heads = n_heads
        self.k_channels = channels // n_heads
        self.window_size = window_size

        self.conv_q = nn.Conv1d(channels, channels, 1)
        self.conv_k = nn.Conv1d(channels, channels, 1)
        self.conv_v = nn.Conv1d(channels, channels, 1)
        self.conv_o = nn.Conv1d(channels, out_channels, 1)
        self.drop = nn.Dropout(p_dropout)

        if window_size is not None:
            n_heads_rel = 1
            rel_stddev = self.k_channels ** -0.5
            self.emb_rel_k = nn.Parameter(
                torch.randn(n_heads_rel, window_size * 2 + 1, self.k_channels) * rel_stddev
            )
            self.emb_rel_v = nn.Parameter(
                torch.randn(n_heads_rel, window_size * 2 + 1, self.k_channels) * rel_stddev
            )

    def forward(self, x, c, attn_mask=None):
        q = self.conv_q(x)
        k = self.conv_k(c)
        v = self.conv_v(c)

        x, _ = self.attention(q, k, v, mask=attn_mask)
        x = self.conv_o(x)
        return x

    def attention(self, query, key, value, mask=None):
        b, d, t_s = key.shape
        t_t = query.shape[2]

        query = query.view(b, self.n_heads, self.k_channels, t_t).transpose(2, 3)
        key = key.view(b, self.n_heads, self.k_channels, t_s).transpose(2, 3)
        value = value.view(b, self.n_heads, self.k_channels, t_s).transpose(2, 3)

        scores = torch.matmul(query / math.sqrt(self.k_channels), key.transpose(-2, -1))

        if self.window_size is not None:
            key_relative_embeddings = self._get_relative_embeddings(self.emb_rel_k, t_s)
            rel_logits = self._matmul_with_relative_keys(
                query / math.sqrt(self.k_channels), key_relative_embeddings
            )
            scores_local = self._relative_position_to_absolute_position(rel_logits)
            scores = scores + scores_local

        if mask is not None:
            scores = scores.masked_fill(mask == 0, -1e4)

        p_attn = self.drop(F.softmax(scores, dim=-1))
        output = torch.matmul(p_attn, value)

        if self.window_size is not None:
            relative_weights = self._absolute_position_to_relative_position(p_attn)
            value_relative_embeddings = self._get_relative_embeddings(self.emb_rel_v, t_s)
            output = output + self._matmul_with_relative_values(
                relative_weights, value_relative_embeddings
            )

        output = output.transpose(2, 3).contiguous().view(b, d, t_t)
        return output, p_attn

    def _matmul_with_relative_values(self, x, y):
        return torch.matmul(x, y.unsqueeze(0))

    def _matmul_with_relative_keys(self, x, y):
        return torch.matmul(x, y.unsqueeze(0).transpose(-2, -1))

    def _get_relative_embeddings(self, relative_embeddings, length):
        pad_length = max(length - (self.window_size + 1), 0)
        slice_start = max((self.window_size + 1) - length, 0)
        slice_end = slice_start + 2 * length - 1
        if pad_length > 0:
            padded = F.pad(relative_embeddings, [0, 0, pad_length, pad_length, 0, 0])
        else:
            padded = relative_embeddings
        return padded[:, slice_start:slice_end]

    def _relative_position_to_absolute_position(self, x):
        batch, heads, length, _ = x.size()
        x = F.pad(x, [0, 1, 0, 0, 0, 0, 0, 0])
        x_flat = x.view([batch, heads, length * 2 * length])
        x_flat = F.pad(x_flat, [0, length - 1, 0, 0, 0, 0])
        return x_flat.view([batch, heads, length + 1, 2 * length - 1])[:, :, :length, length - 1:]

    def _absolute_position_to_relative_position(self, x):
        batch, heads, length, _ = x.size()
        x = F.pad(x, [0, length - 1, 0, 0, 0, 0, 0, 0])
        x_flat = x.view([batch, heads, length ** 2 + length * (length - 1)])
        x_flat = F.pad(x_flat, [length, 0, 0, 0, 0, 0])
        return x_flat.view([batch, heads, length, 2 * length])[:, :, :, 1:]


class FFN(nn.Module):
    def __init__(self, channels, filter_channels, kernel_size, p_dropout):
        super().__init__()
        self.conv_1 = nn.Conv1d(channels, filter_channels, kernel_size,
                               padding=kernel_size // 2)
        self.conv_2 = nn.Conv1d(filter_channels, channels, kernel_size,
                               padding=kernel_size // 2)
        self.drop = nn.Dropout(p_dropout)

    def forward(self, x, x_mask):
        x = self.conv_1(x * x_mask)
        x = torch.relu(x)
        x = self.drop(x)
        x = self.conv_2(x * x_mask)
        return x * x_mask


class GeneratorNSF(nn.Module):
    """NSF-based audio generator with harmonic + noise synthesis."""

    def __init__(self, initial_channel, resblock_kernel_sizes,
                 resblock_dilation_sizes, upsample_rates,
                 upsample_initial_channel, upsample_kernel_sizes,
                 gin_channels, sr, is_half=False):
        super().__init__()
        self.num_kernels = len(resblock_kernel_sizes)
        self.num_upsamples = len(upsample_rates)

        self.f0_upsamp = nn.Upsample(scale_factor=math.prod(upsample_rates))
        self.m_source = SourceModuleHnNSF(sampling_rate=sr, is_half=is_half)

        self.conv_pre = nn.Conv1d(initial_channel, upsample_initial_channel, 7, 1, 3)
        self.noise_convs = nn.ModuleList()
        self.ups = nn.ModuleList()

        for i, (u, k) in enumerate(zip(upsample_rates, upsample_kernel_sizes)):
            c_cur = upsample_initial_channel // (2 ** (i + 1))
            self.ups.append(
                weight_norm(nn.ConvTranspose1d(
                    upsample_initial_channel // (2 ** i),
                    c_cur, k, u, padding=(k - u) // 2
                ))
            )

            if i + 1 < len(upsample_rates):
                stride_f0 = math.prod(upsample_rates[i + 1:])
                self.noise_convs.append(
                    nn.Conv1d(1, c_cur, kernel_size=stride_f0 * 2,
                             stride=stride_f0, padding=stride_f0 // 2)
                )
            else:
                self.noise_convs.append(nn.Conv1d(1, c_cur, kernel_size=1))

        self.resblocks = nn.ModuleList()
        for i in range(len(self.ups)):
            ch = upsample_initial_channel // (2 ** (i + 1))
            for k, d in zip(resblock_kernel_sizes, resblock_dilation_sizes):
                self.resblocks.append(ResBlock(ch, k, d))

        self.conv_post = nn.Conv1d(ch, 1, 7, 1, 3, bias=False)
        self.cond = nn.Conv1d(gin_channels, upsample_initial_channel, 1)

    def forward(self, x, f0, g=None):
        f0 = self.f0_upsamp(f0[:, None]).transpose(1, 2)
        har_source, _, _ = self.m_source(f0)
        har_source = har_source.transpose(1, 2)

        x = self.conv_pre(x)
        if g is not None:
            x = x + self.cond(g)

        for i in range(self.num_upsamples):
            x = F.leaky_relu(x, 0.1)
            x = self.ups[i](x)
            x_source = self.noise_convs[i](har_source)
            x = x + x_source

            xs = None
            for j in range(self.num_kernels):
                idx = i * self.num_kernels + j
                if idx < len(self.resblocks):
                    if xs is None:
                        xs = self.resblocks[idx](x)
                    else:
                        xs += self.resblocks[idx](x)

            x = xs / self.num_kernels if xs is not None else x

        x = F.leaky_relu(x)
        x = self.conv_post(x)
        x = torch.tanh(x)
        return x


class SourceModuleHnNSF(nn.Module):
    """Harmonic + noise source module for NSF."""

    def __init__(self, sampling_rate, harmonic_num=0, sine_amp=0.1,
                 add_noise_std=0.003, voiced_threshold=0, is_half=False):
        super().__init__()
        self.sine_amp = sine_amp
        self.noise_std = add_noise_std
        self.is_half = is_half
        self.l_sin_gen = SineGen(sampling_rate, harmonic_num, sine_amp,
                                add_noise_std, voiced_threshold)
        self.l_linear = nn.Linear(harmonic_num + 1, 1)
        self.l_tanh = nn.Tanh()

    def forward(self, x, upp=None):
        sine_wavs, uv, _ = self.l_sin_gen(x, upp)
        if self.is_half:
            sine_wavs = sine_wavs.half()
        sine_merge = self.l_tanh(self.l_linear(sine_wavs))
        noise = torch.randn_like(sine_merge) * self.sine_amp / 3
        return sine_merge, noise, uv


class SineGen(nn.Module):
    """Sine wave generator for harmonic synthesis."""

    def __init__(self, samp_rate, harmonic_num=0, sine_amp=0.1,
                 noise_std=0.003, voiced_threshold=0):
        super().__init__()
        self.sine_amp = sine_amp
        self.noise_std = noise_std
        self.harmonic_num = harmonic_num
        self.sampling_rate = samp_rate
        self.voiced_threshold = voiced_threshold
        self.dim = harmonic_num + 1

    def forward(self, f0, upp=None):
        with torch.no_grad():
            fn = f0 * torch.arange(1, self.dim + 1, device=f0.device).float()
            rad_values = fn / self.sampling_rate
            rand_ini = torch.rand(fn.shape[0], fn.shape[2], device=fn.device)
            rand_ini[:, 0] = 0
            rad_values[:, 0, 1:] = rad_values[:, 0, 1:] + rand_ini[:, 1:]
            tmp_over_one = torch.cumsum(rad_values, dim=1) % 1
            tmp_over_one_idx = (tmp_over_one[:, 1:, :] - tmp_over_one[:, :-1, :]) < 0
            cumsum_shift = F.pad(tmp_over_one_idx.float(), (0, 0, 1, 0), mode="constant", value=0)
            sines = torch.sin(torch.cumsum(rad_values - cumsum_shift, dim=1) * 2 * math.pi)

        uv = (f0 > self.voiced_threshold).float()
        noise_amp = uv * self.noise_std + (1 - uv) * self.sine_amp / 3
        noise = noise_amp * torch.randn_like(sines)
        sines = sines * self.sine_amp * uv + noise
        return sines, uv, noise


class ResBlock(nn.Module):
    def __init__(self, channels, kernel_size=3, dilation=(1, 3, 5)):
        super().__init__()
        self.convs1 = nn.ModuleList([
            weight_norm(nn.Conv1d(channels, channels, kernel_size, 1,
                                dilation=d, padding=d * (kernel_size - 1) // 2))
            for d in dilation
        ])
        self.convs2 = nn.ModuleList([
            weight_norm(nn.Conv1d(channels, channels, kernel_size, 1,
                                dilation=1, padding=(kernel_size - 1) // 2))
            for _ in dilation
        ])

    def forward(self, x):
        for c1, c2 in zip(self.convs1, self.convs2):
            xt = F.leaky_relu(x, 0.1)
            xt = c1(xt)
            xt = F.leaky_relu(xt, 0.1)
            xt = c2(xt)
            x = xt + x
        return x


class PosteriorEncoder(nn.Module):
    def __init__(self, in_channels, out_channels, hidden_channels,
                 kernel_size, dilation_rate, n_layers, gin_channels=0):
        super().__init__()
        self.out_channels = out_channels
        self.pre = nn.Conv1d(in_channels, hidden_channels, 1)
        self.enc = WN(hidden_channels, kernel_size, dilation_rate, n_layers,
                     gin_channels=gin_channels)
        self.proj = nn.Conv1d(hidden_channels, out_channels * 2, 1)

    def forward(self, x, x_lengths, g=None):
        x_mask = torch.ones(x.shape[0], 1, x.shape[2], device=x.device)
        x = self.pre(x) * x_mask
        x = self.enc(x, x_mask, g=g)
        stats = self.proj(x) * x_mask
        m, logs = torch.split(stats, self.out_channels, dim=1)
        z = (m + torch.exp(logs) * torch.randn_like(m)) * x_mask
        return z, m, logs, x_mask


class WN(nn.Module):
    """WaveNet-style dilated convolution stack."""

    def __init__(self, hidden_channels, kernel_size, dilation_rate, n_layers,
                 gin_channels=0):
        super().__init__()
        self.n_layers = n_layers
        self.in_layers = nn.ModuleList()
        self.res_skip_layers = nn.ModuleList()

        if gin_channels > 0:
            self.cond_layer = weight_norm(nn.Conv1d(gin_channels, 2 * hidden_channels * n_layers, 1))

        for i in range(n_layers):
            dilation = dilation_rate ** i
            padding = int((kernel_size * dilation - dilation) / 2)
            self.in_layers.append(
                weight_norm(nn.Conv1d(hidden_channels, 2 * hidden_channels,
                                    kernel_size, dilation=dilation, padding=padding))
            )
            if i < n_layers - 1:
                res_skip_channels = 2 * hidden_channels
            else:
                res_skip_channels = hidden_channels
            self.res_skip_layers.append(
                weight_norm(nn.Conv1d(hidden_channels, res_skip_channels, 1))
            )

    def forward(self, x, x_mask, g=None):
        output = torch.zeros_like(x)
        if g is not None:
            g = self.cond_layer(g)

        for i in range(self.n_layers):
            x_in = self.in_layers[i](x)
            if g is not None:
                cond_offset = i * 2 * x.shape[1]
                g_l = g[:, cond_offset:cond_offset + 2 * x.shape[1], :]
                x_in = x_in + g_l

            t_act = torch.tanh(x_in[:, :x.shape[1], :])
            s_act = torch.sigmoid(x_in[:, x.shape[1]:, :])
            acts = t_act * s_act

            res_skip_acts = self.res_skip_layers[i](acts)

            if i < self.n_layers - 1:
                x = (x + res_skip_acts[:, :x.shape[1], :]) * x_mask
                output = output + res_skip_acts[:, x.shape[1]:, :]
            else:
                output = output + res_skip_acts

        return output * x_mask


class ResidualCouplingBlock(nn.Module):
    """Normalizing flow for latent space transformation."""

    def __init__(self, channels, hidden_channels, kernel_size, dilation_rate,
                 n_layers, n_flows=4, gin_channels=0):
        super().__init__()
        self.flows = nn.ModuleList()
        for _ in range(n_flows):
            self.flows.append(
                ResidualCouplingLayer(channels, hidden_channels, kernel_size,
                                    dilation_rate, n_layers, gin_channels=gin_channels)
            )
            self.flows.append(Flip())

    def forward(self, x, x_mask, g=None, reverse=False):
        if not reverse:
            for flow in self.flows:
                x, _ = flow(x, x_mask, g=g, reverse=reverse)
        else:
            for flow in reversed(self.flows):
                x = flow(x, x_mask, g=g, reverse=reverse)
        return x


class ResidualCouplingLayer(nn.Module):
    def __init__(self, channels, hidden_channels, kernel_size, dilation_rate,
                 n_layers, gin_channels=0, mean_only=True):
        super().__init__()
        self.half_channels = channels // 2
        self.mean_only = mean_only

        self.pre = nn.Conv1d(self.half_channels, hidden_channels, 1)
        self.enc = WN(hidden_channels, kernel_size, dilation_rate, n_layers,
                     gin_channels=gin_channels)
        self.post = nn.Conv1d(hidden_channels, self.half_channels * (1 if mean_only else 2), 1)
        self.post.weight.data.zero_()
        self.post.bias.data.zero_()

    def forward(self, x, x_mask, g=None, reverse=False):
        x0, x1 = torch.split(x, [self.half_channels] * 2, 1)
        h = self.pre(x0) * x_mask
        h = self.enc(h, x_mask, g=g)
        stats = self.post(h) * x_mask
        m = stats
        logs = torch.zeros_like(m)

        if not reverse:
            x1 = m + x1 * torch.exp(logs) * x_mask
            x = torch.cat([x0, x1], 1)
            return x, torch.sum(logs, [1, 2])
        else:
            x1 = (x1 - m) * torch.exp(-logs) * x_mask
            x = torch.cat([x0, x1], 1)
            return x


class Flip(nn.Module):
    def forward(self, x, *args, reverse=False, **kwargs):
        x = torch.flip(x, [1])
        if not reverse:
            return x, torch.zeros(x.size(0), device=x.device)
        return x


class SynthesizerTrnMs768NSFsid(nn.Module):
    """RVC v2 Voice Conversion Network - main inference model."""

    def __init__(self, spec_channels, segment_size, inter_channels,
                 hidden_channels, filter_channels, n_heads, n_layers,
                 kernel_size, p_dropout, resblock, resblock_kernel_sizes,
                 resblock_dilation_sizes, upsample_rates,
                 upsample_initial_channel, upsample_kernel_sizes,
                 spk_embed_dim, gin_channels, sr, **kwargs):
        super().__init__()
        self.spec_channels = spec_channels
        self.inter_channels = inter_channels
        self.hidden_channels = hidden_channels
        self.segment_size = segment_size
        self.sr = sr

        self.enc_p = TextEncoder768(
            inter_channels, hidden_channels, filter_channels,
            n_heads, n_layers, kernel_size, p_dropout, f0=True
        )
        self.dec = GeneratorNSF(
            inter_channels, resblock_kernel_sizes, resblock_dilation_sizes,
            upsample_rates, upsample_initial_channel, upsample_kernel_sizes,
            gin_channels, sr
        )
        self.enc_q = PosteriorEncoder(
            spec_channels, inter_channels, hidden_channels, 5, 1, 16,
            gin_channels=gin_channels
        )
        self.flow = ResidualCouplingBlock(
            inter_channels, hidden_channels, 5, 1, 3,
            gin_channels=gin_channels
        )
        self.emb_g = nn.Embedding(spk_embed_dim, gin_channels)

    def infer(self, phone, phone_lengths, pitch, nsff0, sid,
              skip_head=None, return_length=None):
        g = self.emb_g(sid).unsqueeze(-1)
        m_p, logs_p, x_mask = self.enc_p(phone, pitch, phone_lengths)
        z_p = (m_p + torch.exp(logs_p) * torch.randn_like(m_p) * 0.66666) * x_mask
        z = self.flow(z_p, x_mask, g=g, reverse=True)
        o = self.dec(z * x_mask, nsff0, g=g)
        return o, x_mask, (z, z_p, m_p, logs_p)
