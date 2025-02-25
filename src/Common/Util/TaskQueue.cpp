#include "Common/Util/TaskQueue.h"
#include "Common/Debug/Logger.h"

#include <vector>
#include <future>

void TaskQueue::AddTask(const std::function<bool()>& task) {
	mTaskQueue.push(task);
}

BOOL TaskQueue::Run(UINT64 numThreads) {
	std::vector<std::future<BOOL>> threads;

	for (UINT64 i = 0; i < numThreads; ++i)
		threads.emplace_back(std::async(std::launch::async, &TaskQueue::ExecuteTasks, this));


	BOOL status = TRUE;
	for (UINT64 i = 0; i < numThreads; ++i)
		status = status && threads[i].get();

	return status;
}

BOOL TaskQueue::ExecuteTasks() {
	while (TRUE) {
		std::function<BOOL()> task;
		{
			std::lock_guard<std::mutex> lock(mMutex);
			if (mTaskQueue.empty()) break;
			task = std::move(mTaskQueue.front());
			mTaskQueue.pop();
		}
		CheckReturn(task());
	}

	return TRUE;
}