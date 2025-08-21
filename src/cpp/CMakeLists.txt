# Add all header files
set(HEADER_FILES
    include/order_flow_predictor.hpp
)

# Add all source files
set(SOURCE_FILES
    order_flow_predictor.cpp
)

# Create library
add_library(order_flow_predictor
    ${SOURCE_FILES}
    ${HEADER_FILES}
)

# Include directories
target_include_directories(order_flow_predictor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link with Conan dependencies if using Conan
if(USE_CONAN)
    target_link_libraries(order_flow_predictor ${CONAN_LIBS})
endif()
