#include "Skybox.hlsli"

// Skybox用の頂点入力構造体
struct VertexShaderInput {
    float32_t4 position : POSITION0;
};

// 変換行列を格納する定数バッファ
struct TransformationMatrix {
    matrix WVP;   // World * View * Projection
    matrix World;
    matrix WorldInverseTranspose;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b1);

VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;
    // WVP変換後、z=wにして深度が常に1になるようにする
    output.position = mul(input.position, gTransformationMatrix.WVP).xyww;
    // 頂点位置をそのままcubemapサンプリング用の方向ベクトルとして出力
    output.texcoord = input.position.xyz;
    return output;
}
