#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

class RenderRecordingWorker
{
public:
	RenderRecordingWorker() = default;
	~RenderRecordingWorker();

	void Start();
	void Wait();
	void Stop();
	bool Submit(std::function<void()> Job);

private:
	void ThreadMain();

	std::thread WorkerThread;
	std::mutex JobMutex;
	std::condition_variable JobAvailable;
	std::condition_variable JobFinished;
	std::function<void()> PendingJob;

	bool bStop = false;
	bool bBusy = false;
};