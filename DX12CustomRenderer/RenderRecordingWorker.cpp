#include "RenderRecordingWorker.h"

RenderRecordingWorker::~RenderRecordingWorker()
{
    Stop();
}

void RenderRecordingWorker::Start()
{
    if (WorkerThread.joinable())
    {
        return;
    }
    {
        std::lock_guard<std::mutex> Lock(JobMutex);
        bStop = false;
    }

    WorkerThread = std::move(std::thread([this] {
        ThreadMain();
	        }
        )
    );
}

void RenderRecordingWorker::Wait()
{
    {
        std::unique_lock<std::mutex> Lock(JobMutex);
        JobFinished.wait(Lock, [this]
            {
                //접수된 작업의 실행까지 모두 끝났는지 반환
                return !bBusy;
            });
    }
}

void RenderRecordingWorker::Stop()
{
    if (!WorkerThread.joinable())
    {
        return; 
    }
    {
        std::lock_guard<std::mutex> Lock(JobMutex);
		bStop = true;
    }
	JobAvailable.notify_one();
    WorkerThread.join();
}

bool RenderRecordingWorker::Submit(std::function<void()> Job)
{
	{
		std::unique_lock<std::mutex> Lock(JobMutex);
		if (!Job || bBusy || bStop)
		{
			return false;
		}
		PendingJob = std::move(Job);
		bBusy = true;
	}
	JobAvailable.notify_one();
	return true;
}

void RenderRecordingWorker::ThreadMain()
{
    while (true)
    {
        std::function<void()> Job;
        {
            std::unique_lock<std::mutex> Lock(JobMutex);

            JobAvailable.wait(Lock, [this]
                {
                    return bStop || static_cast<bool>(PendingJob);
                });

            if (bStop && !PendingJob)
            {
                return;
            }

            //PendingJob을 지역변수 Job으로 이동
            Job = std::move(PendingJob);
            PendingJob = nullptr;
        }

        //Job 실행
        Job();
        Job = nullptr;
        {
            std::lock_guard<std::mutex> Lock(JobMutex);
            bBusy = false;
        }

        JobFinished.notify_all();
    }
}
