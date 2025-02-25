#pragma once

#include <functional>
#include <queue>
#include <mutex>
#include <Windows.h>

class TaskQueue {
public:
	TaskQueue() = default;
	virtual ~TaskQueue() = default;

public:
	void AddTask(const std::function<bool()>& task);

	BOOL Run(UINT64 numThreads);

private:
	BOOL ExecuteTasks();

private:
	std::queue<std::function<bool()>> mTaskQueue;

	std::mutex mMutex;
};