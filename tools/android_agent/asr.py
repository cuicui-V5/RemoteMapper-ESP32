import argparse
import json
import time
import wave
from pathlib import Path

import mlx_whisper
import numpy as np
import torch
from silero_vad import collect_chunks, get_speech_timestamps, load_silero_vad


def transcribe(path: Path, model: Path):
    with wave.open(str(path), "rb") as source:
        if (source.getnchannels(), source.getsampwidth(), source.getframerate()) != (1, 2, 16000):
            raise ValueError("录音必须为 16 kHz、16 bit、单声道 PCM WAV")
        frames = source.getnframes()
        if not 3200 <= frames <= 16000 * 90:
            raise ValueError("录音长度需要介于 0.2 秒与 90 秒之间")
        samples = np.frombuffer(source.readframes(frames), dtype="<i2").astype(np.float32) / 32768.0
    if samples.size != frames:
        raise ValueError("WAV 录音数据不完整")
    peak = float(np.max(np.abs(samples)))
    rms = float(np.sqrt(np.mean(samples * samples)))
    if peak < 0.003 or rms < 0.0003:
        raise ValueError("录音没有足够的声音，任务尚未提交")
    started = time.monotonic()
    torch.set_num_threads(1)
    audio = torch.from_numpy(samples)
    speech = get_speech_timestamps(audio, load_silero_vad(), sampling_rate=16000, min_silence_duration_ms=300, speech_pad_ms=200)
    if not speech:
        raise ValueError("录音没有检测到讲话，任务尚未提交")
    voiced = collect_chunks(speech, audio).numpy()
    result = mlx_whisper.transcribe(voiced, path_or_hf_repo=str(model.resolve()), language="zh", temperature=0.0, condition_on_previous_text=False, verbose=False)
    text = result["text"].strip()
    if not text or not result["segments"]:
        raise ValueError("语音识别没有返回文字，任务尚未提交")
    segments = [{key: segment[key] for key in ("start", "end", "text", "avg_logprob", "no_speech_prob")} for segment in result["segments"]]
    if all(segment["no_speech_prob"] > 0.6 or segment["avg_logprob"] < -1.0 for segment in segments):
        raise ValueError("语音识别置信度不足，任务尚未提交")
    return {"text": text, "seconds": frames / 16000, "speech_seconds": voiced.size / 16000, "speech_ranges": speech, "peak": peak, "rms": rms, "elapsed_seconds": round(time.monotonic() - started, 3), "segments": segments}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("audio", type=Path)
    parser.add_argument("--model", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    result = transcribe(args.audio, args.model)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
