#include "gtest/gtest.h"
#include "JobSystem.h"

namespace spt::js::tests
{

TEST(JobSystemTest, SimpleJob)
{
	std::atomic<Bool> finished = false;

	Job job = Launch(SPT_GENERIC_JOB_NAME,
							 [&finished]
							 {
								finished.store(true);
							 });

	// Don't call Wait() in this case, this is separate case which will be tested in another test
	std::this_thread::sleep_for(std::chrono::milliseconds(5));

	EXPECT_EQ(finished.load(), true);
}


TEST(JobSystemTest, SimpleJobWait)
{
	std::atomic<Bool> finished = false;

	Job job = Launch(SPT_GENERIC_JOB_NAME,
					 [&finished]
					 {
						 std::this_thread::sleep_for(std::chrono::milliseconds(2));
						 finished.store(true);
					 });

	job.Wait();

	EXPECT_EQ(finished.load(), true);
}


TEST(JobSystemTest, JobWithPrerequisite)
{
	std::atomic<Uint32> counter = 0u;

	std::atomic<Uint32> counterAtPrerequisite = 0u;
	std::atomic<Uint32> counterAtJob = 0u;

	Job job1 = Launch(SPT_GENERIC_JOB_NAME,
					  [&counter, &counterAtPrerequisite]
					  {
						  std::this_thread::sleep_for(std::chrono::milliseconds(2));
						  const Uint32 val = counter.fetch_add(1u);
						  counterAtPrerequisite.store(val);
					  });

	Job job2 = Launch(SPT_GENERIC_JOB_NAME,
					  [&counter, &counterAtJob]
					  {
						  const Uint32 val = counter.fetch_add(1u);
						  counterAtJob.store(val);
					  },
					  Prerequisites(job1));

	job2.Wait();

	EXPECT_EQ(counterAtPrerequisite.load(), 0u);
	EXPECT_EQ(counterAtJob.load(), 1u);
	EXPECT_EQ(counter.load(), 2u);
}


TEST(JobSystemTest, InlineJob)
{
	std::atomic<Uint32> counter = 0u;

	LaunchInline(SPT_GENERIC_JOB_NAME,
				[&counter]
				{
					counter.fetch_add(1u);
				});

	EXPECT_EQ(counter.load(), 1u);
}


TEST(JobSystemTest, NestedJobs)
{
	std::atomic<Uint32> counter = 0u;

	constexpr Uint32 nestedJobsNum = 10u;

	Job job = Launch(SPT_GENERIC_JOB_NAME,
					 [&counter]
					 {
						 for (Uint32 idx = 0u; idx < nestedJobsNum; ++idx)
						 {
							 AddNested(SPT_GENERIC_JOB_NAME,
									   [&counter]
									   {
										   counter.fetch_add(1u);
									   });
						 }
					 });

	job.Wait();

	EXPECT_EQ(counter.load(), nestedJobsNum);
}


TEST(JobSystemTest, InlineNestedJobs)
{
	std::atomic<Uint32> counter = 0u;

	constexpr Uint32 nestedJobsNum = 10u;

	LaunchInline(SPT_GENERIC_JOB_NAME,
				 [&counter]
				 {
					 for (Uint32 idx = 0u; idx < nestedJobsNum; ++idx)
					 {
						 AddNested(SPT_GENERIC_JOB_NAME,
								   [&counter]
								   {
									   counter.fetch_add(1u);
								   });
					 }
				 });

	EXPECT_EQ(counter.load(), nestedJobsNum);
}


TEST(JobSystemTest, JobWithResultValue)
{
	constexpr Uint32 expectedResult = 42u;

	JobWithResult job = Launch(SPT_GENERIC_JOB_NAME,
							   []
							   {
								   return expectedResult;
							   });

	const Uint32 result = job.Await();

	EXPECT_EQ(result, expectedResult);
}


TEST(JobSystemTest, ParallelFor)
{
	lib::StaticArray<Bool, 50u> values{};

	Job job = ParallelForEach(SPT_GENERIC_JOB_NAME, values, [](Bool& val) { val = true; });

	job.Wait();

	for (const auto& val : values)
	{
		EXPECT_EQ(val, true);
	}
}


TEST(JobSystemTest, InlineParallelFor)
{
	lib::StaticArray<Bool, 50u> values{};

	InlineParallelForEach(SPT_GENERIC_JOB_NAME, values, [](Bool& val) { val = true; });

	for (const auto& val : values)
	{
		EXPECT_EQ(val, true);
	}
}


TEST(JobSystemTest, JobThenJob)
{
	std::atomic<Uint32> counter = 1u;

	std::atomic<Uint32> counterAtFirstJob = 0u;
	std::atomic<Uint32> counterAtSecondJob = 0u;
	std::atomic<Uint32> counterAtThirdJob = 0u;

	Launch(SPT_GENERIC_JOB_NAME,
		   [&counter, &counterAtFirstJob]
		   {
			   const Uint32 val = counter.fetch_add(1u);
			   counterAtFirstJob.store(val);
		   })
		.Then(SPT_GENERIC_JOB_NAME,
			  [&counter, &counterAtSecondJob]
			  {
				  const Uint32 val = counter.fetch_add(1u);
				  counterAtSecondJob.store(val);
			  })
		.Then(SPT_GENERIC_JOB_NAME,
			  [&counter, &counterAtThirdJob]
			  {
				  const Uint32 val = counter.fetch_add(1u);
				  counterAtThirdJob.store(val);
			  })
		.Wait();

	EXPECT_EQ(counterAtFirstJob.load(), 1u);
	EXPECT_EQ(counterAtSecondJob.load(), 2u);
	EXPECT_EQ(counterAtThirdJob.load(), 3u);
	EXPECT_EQ(counter.load(), 4u);
}


TEST(JobSystemTest, DiamondScenario)
{
	std::atomic<Uint32> counter = 1u;

	std::atomic<Uint32> counterAtFirstJob = 0u;
	std::atomic<Uint32> counterAtSecondJob = 0u;
	std::atomic<Uint32> counterAtThirdJob = 0u;
	std::atomic<Uint32> counterAtFourthJob = 0u;

	Job job1 = Launch(SPT_GENERIC_JOB_NAME,
					  [&counter, &counterAtFirstJob]
					  {
						  const Uint32 val = counter.fetch_add(1u);
						  counterAtFirstJob.store(val);
					  });

	Job job2 = Launch(SPT_GENERIC_JOB_NAME,
					  [&counter, &counterAtSecondJob]
					  {
						  const Uint32 val = counter.fetch_add(1u);
						  counterAtSecondJob.store(val);
					  },
					  Prerequisites(job1));

	Job job3 = Launch(SPT_GENERIC_JOB_NAME,
					  [&counter, &counterAtThirdJob]
					  {
						  const Uint32 val = counter.fetch_add(1u);
						  counterAtThirdJob.store(val);
					  },
					  Prerequisites(job1));

	Launch(SPT_GENERIC_JOB_NAME,
		   [&counter, &counterAtFourthJob]
		   {
			   const Uint32 val = counter.fetch_add(1u);
			   counterAtFourthJob.store(val);
		   },
		   Prerequisites(job2, job3))
		.Wait();

	EXPECT_EQ(counterAtFirstJob.load(), 1u);

	EXPECT_GT(counterAtSecondJob.load(), 1u);
	EXPECT_GT(counterAtThirdJob.load(), 1u);

	EXPECT_LT(counterAtSecondJob.load(), 4u);
	EXPECT_LT(counterAtThirdJob.load(), 4u);

	EXPECT_EQ(counter.load(), 5u);
}

} // spt::js::tests


int main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);

	using namespace spt;

	js::JobSystemInitializationParams jobSystemInitParams;
	jobSystemInitParams.workerThreadsNum = static_cast<SizeType>(std::thread::hardware_concurrency() - 1u);
	js::JobSystem::Initialize(jobSystemInitParams);

	const auto testsResult = RUN_ALL_TESTS();

	js::JobSystem::Shutdown();

	return testsResult;
}