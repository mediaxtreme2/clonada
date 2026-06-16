FROM nvidia/cuda:11.8.0-runtime-ubuntu22.04

WORKDIR /clonada_core

ENV DEBIAN_FRONTEND=noninteractive
ENV PYTHONUNBUFFERED=1

RUN apt-get update && apt-get install -y \
    python3.10 \
    python3-pip \
    ffmpeg \
    git \
    wget \
    && rm -rf /var/lib/apt/lists/*

# Python dependencies
COPY requirements.txt .
RUN pip3 install --no-cache-dir -r requirements.txt

# Copy inference pipeline (copied by CI before build)
COPY python /clonada_core/python

# Pre-fetch RVC-compatible HuBERT (content-vec-768, NOT facebook/hubert-base-ls960)
RUN mkdir -p /clonada_core/weights && \
    wget -q https://huggingface.co/lj1995/VoiceConversionWebUI/resolve/main/hubert_base.pt \
    -O /clonada_core/weights/hubert_base.pt

# Pre-fetch RMVPE weights
RUN mkdir -p /clonada_core/weights && \
    wget -q https://huggingface.co/lj1995/VoiceConversionWebUI/resolve/main/rmvpe.pt \
    -O /clonada_core/weights/rmvpe.pt

# Pre-fetch RVC v2 pretrained weights for training
RUN mkdir -p /clonada_core/weights/pretrained_v2 && \
    wget -q https://huggingface.co/lj1995/VoiceConversionWebUI/resolve/main/pretrained_v2/f0G40k.pth \
    -O /clonada_core/weights/pretrained_v2/f0G40k.pth && \
    wget -q https://huggingface.co/lj1995/VoiceConversionWebUI/resolve/main/pretrained_v2/f0D40k.pth \
    -O /clonada_core/weights/pretrained_v2/f0D40k.pth

COPY handler.py .

CMD ["python3", "-u", "/clonada_core/handler.py"]
