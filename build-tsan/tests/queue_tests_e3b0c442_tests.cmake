add_test([=[FastQueueCorrectness.SingleThreadedPushPop]=]  /Users/abhishek2x/Documents/Learnings/Quant/projects/liquid-book/build-tsan/tests/queue_tests [==[--gtest_filter=FastQueueCorrectness.SingleThreadedPushPop]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[FastQueueCorrectness.SingleThreadedPushPop]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/Users/abhishek2x/Documents/Learnings/Quant/projects/liquid-book/tests/queue_test.cpp:7]==]
    WORKING_DIRECTORY [==[/Users/abhishek2x/Documents/Learnings/Quant/projects/liquid-book/build-tsan/tests]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[FastQueueCorrectness.PushToFullCapacity]=]  /Users/abhishek2x/Documents/Learnings/Quant/projects/liquid-book/build-tsan/tests/queue_tests [==[--gtest_filter=FastQueueCorrectness.PushToFullCapacity]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[FastQueueCorrectness.PushToFullCapacity]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/Users/abhishek2x/Documents/Learnings/Quant/projects/liquid-book/tests/queue_test.cpp:31]==]
    WORKING_DIRECTORY [==[/Users/abhishek2x/Documents/Learnings/Quant/projects/liquid-book/build-tsan/tests]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[FastQueueCorrectness.PopFromEmpty]=]  /Users/abhishek2x/Documents/Learnings/Quant/projects/liquid-book/build-tsan/tests/queue_tests [==[--gtest_filter=FastQueueCorrectness.PopFromEmpty]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[FastQueueCorrectness.PopFromEmpty]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/Users/abhishek2x/Documents/Learnings/Quant/projects/liquid-book/tests/queue_test.cpp:52]==]
    WORKING_DIRECTORY [==[/Users/abhishek2x/Documents/Learnings/Quant/projects/liquid-book/build-tsan/tests]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[FastQueueCorrectness.IndependentConsumerCursors]=]  /Users/abhishek2x/Documents/Learnings/Quant/projects/liquid-book/build-tsan/tests/queue_tests [==[--gtest_filter=FastQueueCorrectness.IndependentConsumerCursors]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[FastQueueCorrectness.IndependentConsumerCursors]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/Users/abhishek2x/Documents/Learnings/Quant/projects/liquid-book/tests/queue_test.cpp:62]==]
    WORKING_DIRECTORY [==[/Users/abhishek2x/Documents/Learnings/Quant/projects/liquid-book/build-tsan/tests]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[FastQueueCorrectness.WrapAroundBoundary]=]  /Users/abhishek2x/Documents/Learnings/Quant/projects/liquid-book/build-tsan/tests/queue_tests [==[--gtest_filter=FastQueueCorrectness.WrapAroundBoundary]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[FastQueueCorrectness.WrapAroundBoundary]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/Users/abhishek2x/Documents/Learnings/Quant/projects/liquid-book/tests/queue_test.cpp:89]==]
    WORKING_DIRECTORY [==[/Users/abhishek2x/Documents/Learnings/Quant/projects/liquid-book/build-tsan/tests]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
set(queue_tests_TESTS [==[FastQueueCorrectness.SingleThreadedPushPop]==] [==[FastQueueCorrectness.PushToFullCapacity]==] [==[FastQueueCorrectness.PopFromEmpty]==] [==[FastQueueCorrectness.IndependentConsumerCursors]==] [==[FastQueueCorrectness.WrapAroundBoundary]==])
