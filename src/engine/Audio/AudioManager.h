#pragma once
#include <xaudio2.h>
#include <cstdint>
#include <vector>
#include <wrl.h>
#include <memory>
#include <mutex>
#include <list>

#pragma comment(lib, "xaudio2.lib")

struct ChunkHeader {
	char id[4];
	int32_t size;
};

struct RiffHeader {
	ChunkHeader chunk;
	char type[4];
};

struct FormatChunk {
	ChunkHeader chunk;
	WAVEFORMATEX fmt;
};

// 音声データ構造体 (std::vector<BYTE>版)
struct SoundData {
	WAVEFORMATEX wfex;
	std::vector<BYTE> buffer;
};

// ============================
// 再生完了時に自動でDestroyVoiceを呼ぶコールバック
// ============================
class VoiceCallback : public IXAudio2VoiceCallback {
public:
	VoiceCallback() = default;
	~VoiceCallback() = default;

	// ストリーム終端に到達 → 再生完了
	STDMETHOD_(void, OnStreamEnd)() override;

	// --- 以下は使わないが純粋仮想なので実装が必要 ---
	STDMETHOD_(void, OnVoiceProcessingPassStart)(UINT32) override {}
	STDMETHOD_(void, OnVoiceProcessingPassEnd)() override {}
	STDMETHOD_(void, OnBufferStart)(void*) override {}
	STDMETHOD_(void, OnBufferEnd)(void*) override {}
	STDMETHOD_(void, OnLoopEnd)(void*) override {}
	STDMETHOD_(void, OnVoiceError)(void*, HRESULT) override {}

	// コールバックが所属する SourceVoice（再生後に破棄するため保持）
	IXAudio2SourceVoice* sourceVoice = nullptr;
};

// ============================
// 再生中の音声を管理するハンドル
// ============================
struct PlayingVoice {
	IXAudio2SourceVoice* sourceVoice = nullptr;
	std::unique_ptr<VoiceCallback> callback;
	bool isFinished = false; // コールバックから再生完了フラグを立てる
};

class AudioManager {
public:
	AudioManager() = default;
	static AudioManager* GetInstance();
	// 初期化
	void Initialize();
	// 終了処理
	void Finalize();

	// 毎フレーム呼ぶ: 再生済みVoiceの回収
	void Update();

	// 音声読み込み (WAV/MP3/AAC対応)
	SoundData SoundLoadFile(const char* filename);

	// 音声データ解放
	void SoundUnload(SoundData* soundData);

	// 音声再生 (再生完了後に自動で Voice を破棄)
	void SoundPlayWave(const SoundData& soundData);

	// 音声再生 (ループ再生)
	void SoundPlayWaveLoop(const SoundData& soundData);

	// すべての再生中サウンドを停止
	void StopAllSounds();

private:
	IXAudio2* xAudio2_ = nullptr;
	static std::unique_ptr<AudioManager> instance;

	IXAudio2MasteringVoice* masteringVoice_ = nullptr;

	// 再生中のVoiceリスト
	std::list<std::unique_ptr<PlayingVoice>> playingVoices_;
	std::mutex voicesMutex_;

	// 内部: SourceVoice を作成して再生する共通処理
	void PlayInternal(const SoundData& soundData, bool loop);
};