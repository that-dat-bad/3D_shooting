#pragma once

// 航空機の外部武装（ペイロード）インターフェース
class IPayload {
public:
    virtual ~IPayload() = default;

    // 更新
    // deltaTime: デルタタイム
    virtual void Update(float deltaTime) = 0;

    // 発射
    virtual void Fire() = 0;

    // 重量取得
    virtual float GetWeight() const = 0;
    
    // 空気抵抗係数取得
    virtual float GetDragCoeff() const = 0;
};
