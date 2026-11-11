#pragma once
#include"Vector3.h"
namespace MyMath {
	/// <summary>
	/// トランスフォーム(変形)構造体
	/// オブジェクトのスケール、回転、平行移動を管理する
	/// </summary>
	struct Transform {
		/// <summary>スケール値 (x, y, z)</summary>
		Vector3 scale;
		/// <summary>オイラー角回転 (x, y, z)[rad]</summary>
		Vector3 rotate;
		/// <summary>平行移動量 (x, y, z)</summary>
		Vector3 translate;
	};
}