#pragma once
#include <memory>
#include <vector>
#include <functional>
#include <string>

class Enemy;

namespace AI {

// ノードの実行結果ステータス
enum class NodeStatus {
	Success, // 成功
	Failure, // 失敗
	Running  // 実行中
};

// ノード基底クラス
class BTNode {
public:
	virtual ~BTNode() = default;

	// 評価・更新処理
	// deltaTime: 経過時間
	// enemy: 対象の敵
	virtual NodeStatus Tick(float deltaTime, Enemy& enemy) = 0;

	// 状態リセット
	virtual void Reset() {}

	// ノード名取得
	virtual const char* GetName() const = 0;
};

// コンポジットノード基底
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

// セレクターノード
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

// シーケンスノード
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

// デコレーターノード基底
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

// インバーターノード
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

// コンディションノード
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

// ビヘイビアツリー管理クラス
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

