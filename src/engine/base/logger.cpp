#include <Windows.h>
#include "logger.h"
#include <debugapi.h>

namespace logger {
	//ログ用関数
	void Log(const std::string& message) {
#ifdef _DEBUG
		OutputDebugStringA(message.c_str());
#endif
	}
}