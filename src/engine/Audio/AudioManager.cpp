#include "AudioManager.h"
#include <cassert>
#include <string>
#include <vector>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "Mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "Mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

using namespace Microsoft::WRL;

#include "../base/StringUtility.h" 

std::unique_ptr<AudioManager> AudioManager::instance = nullptr;

// ============================
// ============================
void VoiceCallback::OnStreamEnd() {
	// (DestroyVoice はコールバック内で呼ぶとデッドロックの恐れがあるため、
	//  メインスレッドの Update() で回収する)
	if (sourceVoice) {
		sourceVoice->Stop();
	}
}

// ============================
// ============================
AudioManager* AudioManager::GetInstance() {
	if (instance == nullptr) { instance = std::make_unique<AudioManager>(); }
	return instance.get();
}

void AudioManager::Initialize() {
	HRESULT result;

	result = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
	assert(SUCCEEDED(result));

	result = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(result));

	result = xAudio2_->CreateMasteringVoice(&masteringVoice_);
	assert(SUCCEEDED(result));
}

void AudioManager::Finalize() {
	// すべての再生中 Voice を停止・破棄
	StopAllSounds();

	if (masteringVoice_) {
		masteringVoice_->DestroyVoice();
		masteringVoice_ = nullptr;
	}
	if (xAudio2_) {
		xAudio2_->Release();
		xAudio2_ = nullptr;
	}
	MFShutdown();
}

void AudioManager::Update() {
	std::lock_guard<std::mutex> lock(voicesMutex_);

	// 再生が完了した Voice を回収・破棄
	auto it = playingVoices_.begin();
	while (it != playingVoices_.end()) {
		auto& pv = *it;

		XAUDIO2_VOICE_STATE state{};
		pv->sourceVoice->GetState(&state);

		if (state.BuffersQueued == 0) {
			// 再生完了 → DestroyVoice してリストから削除
			pv->sourceVoice->DestroyVoice();
			pv->sourceVoice = nullptr;
			it = playingVoices_.erase(it);
		} else {
			++it;
		}
	}
}

#include <filesystem>
#include <cstdio>

SoundData AudioManager::SoundLoadFile(const char* filename) {
	HRESULT result;
	SoundData soundData = {};

	if (!filename || filename[0] == '\0') {
		return soundData;
	}

	// 1. ファイルパス解決（カレントディレクトリ、プロジェクト相対、絶対パス対応）
	std::filesystem::path resolvedPath = filename;
	if (!std::filesystem::exists(resolvedPath)) {
		// カレントディレクトリが output フォルダの場合などの探索候補
		std::vector<std::filesystem::path> candidates = {
			std::filesystem::path("../../25B_GE_CG") / filename,
			std::filesystem::path("../25B_GE_CG") / filename,
			std::filesystem::path("..") / filename,
			std::filesystem::path("../..") / filename,
			std::filesystem::current_path() / filename
		};
		for (const auto& cand : candidates) {
			if (std::filesystem::exists(cand)) {
				resolvedPath = cand;
				break;
			}
		}
	}

	if (!std::filesystem::exists(resolvedPath)) {
		char warnBuf[512];
		snprintf(warnBuf, sizeof(warnBuf), "[AudioManager] WARNING: Audio file not found: %s\n", filename);
		OutputDebugStringA(warnBuf);
		return soundData;
	}

	// Windowsの完全修飾絶対パスに正規化
	std::wstring filePathW = std::filesystem::absolute(resolvedPath).wstring();

	// 2. SourceReader作成
	ComPtr<IMFSourceReader> pReader;
	result = MFCreateSourceReaderFromURL(filePathW.c_str(), nullptr, &pReader);
	if (FAILED(result) || !pReader) {
		char errBuf[512];
		snprintf(errBuf, sizeof(errBuf), "[AudioManager] ERROR: MFCreateSourceReaderFromURL failed (0x%08X): %ls\n", static_cast<unsigned int>(result), filePathW.c_str());
		OutputDebugStringA(errBuf);
		return soundData;
	}

	// 3. メディアタイプ設定 (PCM)
	ComPtr<IMFMediaType> pPCMType;
	result = MFCreateMediaType(&pPCMType);
	if (FAILED(result) || !pPCMType) {
		return soundData;
	}
	pPCMType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	pPCMType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);

	result = pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pPCMType.Get());
	if (FAILED(result)) {
		return soundData;
	}

	// 4. WaveFormat取得
	ComPtr<IMFMediaType> pOutType;
	result = pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pOutType);
	if (FAILED(result) || !pOutType) {
		return soundData;
	}

	WAVEFORMATEX* waveFormat = nullptr;
	result = MFCreateWaveFormatExFromMFMediaType(pOutType.Get(), &waveFormat, nullptr);
	if (FAILED(result) || !waveFormat) {
		return soundData;
	}

	// 構造体にフォーマット情報をコピー
	soundData.wfex = *waveFormat;
	CoTaskMemFree(waveFormat);

	// 5. PCM波形データの取得
	while (true) {
		ComPtr<IMFSample> pSample;
		DWORD streamIndex = 0, flags = 0;
		LONGLONG llTimeStamp = 0;

		result = pReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &streamIndex, &flags, &llTimeStamp, &pSample);
		if (FAILED(result)) { break; }

		// ストリームの末尾に達したら抜ける
		if (flags & MF_SOURCE_READERF_ENDOFSTREAM) { break; }

		if (pSample) {
			ComPtr<IMFMediaBuffer> pBuffer;
			if (SUCCEEDED(pSample->ConvertToContiguousBuffer(&pBuffer)) && pBuffer) {
				BYTE* pData = nullptr;
				DWORD maxLength = 0, currentLength = 0;
				if (SUCCEEDED(pBuffer->Lock(&pData, &maxLength, &currentLength)) && pData) {
					soundData.buffer.insert(soundData.buffer.end(), pData, pData + currentLength);
					pBuffer->Unlock();
				}
			}
		}
	}

	return soundData;
}

void AudioManager::SoundUnload(SoundData* soundData) {
	soundData->buffer.clear();
	soundData->wfex = {};
}

void AudioManager::SoundPlayWave(const SoundData& soundData) {
	PlayInternal(soundData, false);
}

void AudioManager::SoundPlayWaveLoop(const SoundData& soundData) {
	PlayInternal(soundData, true);
}

void AudioManager::PlayInternal(const SoundData& soundData, bool loop) {
	HRESULT result;

	// コールバックオブジェクトを作成
	auto callback = std::make_unique<VoiceCallback>();

	IXAudio2SourceVoice* pSourceVoice = nullptr;
	result = xAudio2_->CreateSourceVoice(&pSourceVoice, &soundData.wfex, 0,
		XAUDIO2_DEFAULT_FREQ_RATIO, callback.get());
	assert(SUCCEEDED(result));

	// コールバックに SourceVoice を登録
	callback->sourceVoice = pSourceVoice;

	// バッファの設定
	XAUDIO2_BUFFER buf{};
	buf.pAudioData = soundData.buffer.data();
	buf.AudioBytes = static_cast<UINT32>(soundData.buffer.size());
	buf.Flags = XAUDIO2_END_OF_STREAM;

	if (loop) {
		buf.LoopCount = XAUDIO2_LOOP_INFINITE;
	}

	result = pSourceVoice->SubmitSourceBuffer(&buf);
	assert(SUCCEEDED(result));

	result = pSourceVoice->Start();
	assert(SUCCEEDED(result));

	// 再生中リストに追加
	auto playingVoice = std::make_unique<PlayingVoice>();
	playingVoice->sourceVoice = pSourceVoice;
	playingVoice->callback = std::move(callback);

	{
		std::lock_guard<std::mutex> lock(voicesMutex_);
		playingVoices_.push_back(std::move(playingVoice));
	}
}

void AudioManager::StopAllSounds() {
	std::lock_guard<std::mutex> lock(voicesMutex_);

	for (auto& pv : playingVoices_) {
		if (pv->sourceVoice) {
			pv->sourceVoice->Stop();
			pv->sourceVoice->DestroyVoice();
			pv->sourceVoice = nullptr;
		}
	}
	playingVoices_.clear();
}
