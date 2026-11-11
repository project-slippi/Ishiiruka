add_dolphin_test(MMIOTest MMIOTest.cpp)
add_dolphin_test(PageFaultTest PageFaultTest.cpp)
add_dolphin_test(CoreTimingTest CoreTimingTest.cpp)

set(SLIPPI_REPLAY_COMM_TEST_SRCS
	SlippiReplayCommTest.cpp
	${CMAKE_SOURCE_DIR}/Source/Core/Core/Slippi/SlippiReplayComm.cpp
	)
add_dolphin_test_with_libraries(SlippiReplayCommTest "${SLIPPI_REPLAY_COMM_TEST_SRCS}"
	common SlippiLib gtest)
