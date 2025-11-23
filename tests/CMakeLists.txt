add_executable(black_scholes_verification
    black_scholes_verification.cpp
)

target_link_libraries(black_scholes_verification
    PRIVATE
        order_flow_predictor
)

target_include_directories(black_scholes_verification
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/cpp/include
)

add_test(NAME black_scholes_verification COMMAND black_scholes_verification)
