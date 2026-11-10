#pragma once
#include <memory>
#include <vector>
#include <functional>
#include <string>

class Enemy;

namespace AI {

/// @brief ビヘイビアツリーのノード実行結果ステータス
enum class NodeStatus {
	Success, ///< 成功（条件成立、タスク完了など）
	Failure, ///< 失敗（条件不成立、タスク失敗など）
	Running  ///< 実行中（フレームを跨いで継続実行中）
};

/// @brief ビヘイビアツリーのノード基底クラス
class BTNode {
public:
	virtual ~BTNode() = default;

	/// @brief 毎フレームの評価・更新処理
	/// @param deltaTime 前フレームからの経過秒数
	/// @param enemy 対象の敵インスタンス
	/// @return 実行ステータス
	virtual NodeStatus Tick(float deltaTime, Enemy& enemy) = 0;

	/// @brief 状態リセット
	virtual void Reset() {}

	/// @brief ノードの識別名
	virtual const char* GetName() const = 0;
};

/// @brief 複数の子ノードを持つコンポジットノード基底
class CompositeNode : public BTNode {
public:
	virtual ~CompositeNode() = default;

	void AddChild(std::unique_ptr<BTNode> child) {
		if (child) {
			children_.push_back(std::move(child));
		}
	}

	void Reset() override {
		for (auto& child : children_) {
			if (child) {
				child->Reset();
			}
		}
	}

	const std::vector<std::unique_ptr<BTNode>>& GetChildren() const {
		return children_;
	}

protected:
	std::vector<std::unique_ptr<BTNode>> children_;
};

/// @brief セレクターノード（Fallback / OR条件）：
/// 子ノードを左から評価し、SuccessまたはRunningを返したノードで即座に終了する。
/// すべての子ノードがFailureとなった場合のみFailureを返す。
class SelectorNode : public CompositeNode {
public:
	explicit SelectorNode(const char* name = "Selector")
		: name_(name) {}

	NodeStatus Tick(float deltaTime, Enemy& enemy) override {
		for (auto& child : children_) {
			if (!child) continue;
			NodeStatus status = child->Tick(deltaTime, enemy);
			if (status == NodeStatus::Success || status == NodeStatus::Running) {
				return status;
			}
		}
		return NodeStatus::Failure;
	}

	const char* GetName() const override { return name_; }

private:
	const char* name_ = "Selector";
};

/// @brief シーケンスノード（AND条件）：
/// 子ノードを左から順に評価し、FailureまたはRunningを返したノードで即座に終了する。
/// すべての子ノードがSuccessとなった場合のみSuccessを返す。
class SequenceNode : public CompositeNode {
public:
	explicit SequenceNode(const char* name = "Sequence")
		: name_(name) {}

	NodeStatus Tick(float deltaTime, Enemy& enemy) override {
		for (auto& child : children_) {
			if (!child) continue;
			NodeStatus status = child->Tick(deltaTime, enemy);
			if (status == NodeStatus::Failure || status == NodeStatus::Running) {
				return status;
			}
		}
		return NodeStatus::Success;
	}

	const char* GetName() const override { return name_; }

private:
	const char* name_ = "Sequence";
};

/// @brief デコレーターノード基底（1つの子ノードを装飾）
class DecoratorNode : public BTNode {
public:
	virtual ~DecoratorNode() = default;

	void SetChild(std::unique_ptr<BTNode> child) {
		child_ = std::move(child);
	}

	void Reset() override {
		if (child_) {
			child_->Reset();
		}
	}

protected:
	std::unique_ptr<BTNode> child_;
};

/// @brief 成功・失敗を反転するインバーターノード
class InverterNode : public DecoratorNode {
public:
	explicit InverterNode(const char* name = "Inverter")
		: name_(name) {}

	NodeStatus Tick(float deltaTime, Enemy& enemy) override {
		if (!child_) return NodeStatus::Failure;
		NodeStatus status = child_->Tick(deltaTime, enemy);
		if (status == NodeStatus::Success) return NodeStatus::Failure;
		if (status == NodeStatus::Failure) return NodeStatus::Success;
		return NodeStatus::Running;
	}

	const char* GetName() const override { return name_; }

private:
	const char* name_ = "Inverter";
};

/// @brief ラムダ式や述語関数で成否を即座に判定するコンディションノード
class ConditionNode : public BTNode {
public:
	using Predicate = std::function<bool(Enemy& enemy)>;

	ConditionNode(const char* name, Predicate predicate)
		: name_(name), predicate_(std::move(predicate)) {}

	NodeStatus Tick(float deltaTime, Enemy& enemy) override {
		(void)deltaTime;
		if (predicate_ && predicate_(enemy)) {
			return NodeStatus::Success;
		}
		return NodeStatus::Failure;
	}

	const char* GetName() const override { return name_; }

private:
	const char* name_ = "Condition";
	Predicate predicate_;
};

/// @brief ビヘイビアツリー管理クラス
class BehaviorTree {
public:
	BehaviorTree() = default;
	~BehaviorTree() = default;

	void SetRoot(std::unique_ptr<BTNode> root) {
		root_ = std::move(root);
	}

	NodeStatus Tick(float deltaTime, Enemy& enemy) {
		if (!root_) {
			return NodeStatus::Failure;
		}
		return root_->Tick(deltaTime, enemy);
	}

	void Reset() {
		if (root_) {
			root_->Reset();
		}
	}

	BTNode* GetRoot() const { return root_.get(); }

private:
	std::unique_ptr<BTNode> root_;
};

} // namespace AI

