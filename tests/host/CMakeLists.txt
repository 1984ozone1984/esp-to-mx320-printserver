# Host tier: plain C, no ESP-IDF. Pure cores from components/*/ *_core.c are
# added here by /build as they are written (docs/Method/project/architecture.md).
cmake_minimum_required(VERSION 3.22)
project(mx320_host_tests C)
enable_testing()
add_executable(test_tier_runs test_tier_runs.c)
add_test(NAME tier_runs COMMAND test_tier_runs)
