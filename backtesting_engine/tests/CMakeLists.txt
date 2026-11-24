add_executable(backtest_tests
    test_backtester.cpp
    test_cli.cpp
    test_data_parser.cpp
    test_indicators.cpp
    test_metrics.cpp
    test_optimizer.cpp
    test_strategies.cpp
)
target_link_libraries(backtest_tests PRIVATE backtest_core backtest_options GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(backtest_tests DISCOVERY_TIMEOUT 60)
