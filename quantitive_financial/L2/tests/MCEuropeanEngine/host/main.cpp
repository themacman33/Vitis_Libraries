/*
 * Copyright 2019 Xilinx, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef HLS_TEST
#include "xcl2.hpp"
#endif
#include <cstring>
#include <vector>
#include <fstream>
#include <iostream>
#include <sys/time.h>
#include "ap_int.h"
#include "utils.hpp"
#include "mcengine_top.hpp"

class ArgParser {
   public:
    ArgParser(int& argc, const char** argv) {
        for (int i = 1; i < argc; ++i) mTokens.push_back(std::string(argv[i]));
    }
    bool getCmdOption(const std::string option, std::string& value) const {
        std::vector<std::string>::const_iterator itr;
        itr = std::find(this->mTokens.begin(), this->mTokens.end(), option);
        if (itr != this->mTokens.end() && ++itr != this->mTokens.end()) {
            value = *itr;
            return true;
        }
        return false;
    }

   private:
    std::vector<std::string> mTokens;
};

int main(int argc, const char* argv[]) {
    // cmd parser
    ArgParser parser(argc, argv);
    std::string mode;
    std::string xclbin_path;
    std::string mode_emu = "hw";
#ifndef HLS_TEST
    if (!parser.getCmdOption("-xclbin", xclbin_path)) {
        std::cout << "ERROR:xclbin path is not set!\n";
        return 1;
    }

    if (std::getenv("XCL_EMULATION_MODE") != nullptr) {
        mode_emu = std::getenv("XCL_EMULATION_MODE");
    }
    std::cout << "[INFO]Running in " << mode_emu << " mode" << std::endl;
#endif
    // Allocate Memory in Host Memory
    TEST_DT* outputs = aligned_alloc<TEST_DT>(1);
    unsigned int* seed = aligned_alloc<unsigned int>(2);

    // -------------setup k0 params---------------

    bool optionTypes[] = {false, true};
    TEST_DT strikes[] = {75.0, 100.0, 125.0};
    TEST_DT underlyings[] = {100};
    TEST_DT riskFreeRates[] = {0.01, 0.05, 0.15};
    TEST_DT volatilitys[] = {0.11, 0.50, 1.20};
    TEST_DT dividendYields[] = {0.00, 0.05};

    TEST_DT timeLength = 1;
    TEST_DT requiredTolerance = 0.02;

    unsigned int requiredSamples = 40000;
    TEST_DT relative_err = 0.01;
    unsigned int maxSamples = 0;
    unsigned int timeSteps = 1;

    TEST_DT goldens[] = {25.7561,   32.9468, 53.2671, 28.6606,    34.839,  54.3405, 35.447,      39.5876, 56.9929,
                         20.9081,   29.1095, 49.3851, 23.7934,    30.8919, 50.4131, 30.5703,     35.3988, 52.9576,
                         4.87984,   20.1444, 45.4236, 7.15178,    21.7926, 46.5208, 14.334,      26.1325, 49.2591,
                         2.59445,   17.2806, 41.9043, 4.17224,    18.7785, 42.9474, 10.0343,     22.7598, 45.5556,
                         0.122447,  12.135,  39.3347, 0.295976,   13.4076, 40.4184, 1.72776,     16.9103, 43.1452,
                         0.0331968, 10.1321, 36.1351, 0.0918666,  11.2518, 37.1594, 0.733906,    14.3665, 39.7416,
                         0.009834,  7.20058, 27.5209, 0.00276481, 6.18119, 25.6827, 6.92225e-05, 4.14074, 21.546,
                         0.0389132, 8.24025, 28.5159, 0.0126175,  7.11112, 26.6323, 0.000456755, 4.82895, 22.3877,
                         3.88483,   19.1494, 44.4286, 2.27473,    16.9155, 41.6437, 0.404771,    12.2033, 35.3299,
                         6.47649,   21.1627, 45.7864, 4.17224,    18.7785, 42.9474, 0.982153,    13.7077, 36.5034,
                         23.8787,   35.8912, 63.0909, 19.1997,    32.3113, 59.322,  9.13635,     24.4988, 50.7337,
                         28.6665,   38.7653, 64.7684, 23.8726,    35.0325, 60.9401, 13.1995,     26.8321, 52.2071};

    seeds[0] = 1;
    seeds[1] = 10001;

    int idx = 0;
    int opt_len, st_len, unly_len, r_len, d_len, vol_len;
    if (mode == "hw_emu") {
        opt_len = 1;
        st_len = 1;
        unly_len = 1;
        r_len = 1;
        d_len = 1;
        vol_len = 1;
    } else {
        opt_len = LENGTH(optionTypes);
        st_len = LENGTH(strikes);
        unly_len = LENGTH(underlyings);
        r_len = LENGTH(riskFreeRates);
        d_len = LENGTH(dividendYields);
        vol_len = LENGTH(volatilitys);
    }
#ifndef HLS_TEST
    // do pre-process on CPU
    struct timeval start_time, end_time, test_time;
    // platform related operations
    std::vector<cl::Device> devices = xcl::get_xil_devices();
    cl::Device device = devices[0];

    // Creating Context and Command Queue for selected Device
    cl::Context context(device);
    cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE);
    std::string devName = device.getInfo<CL_DEVICE_NAME>();
    printf("Found Device=%s\n", devName.c_str());

    // cl::Program::Binaries xclBins = xcl::import_binary_file("../xclbin/MCAE_u250_hw.xclbin");
    cl::Program::Binaries xclBins = xcl::import_binary_file(xclbin_path);
    devices.resize(1);
    cl::Program program(context, devices, xclBins);
    cl::Kernel kernel_Engine(program, "mc_euro_k");
    std::cout << "kernel has been created" << std::endl;

    cl_mem_ext_ptr_t mext_o[5];
    mext_o[0].flags = XCL_MEM_DDR_BANK0;
    mext_o[0].obj = outputs;
    mext_o[0].param = 0;

    mext_o[1].flags = XCL_MEM_DDR_BANK0;
    mext_o[1].obj = seed;
    mext_o[1].param = 0;

    // create device buffer and map dev buf to host buf
    cl::Buffer output_buf;
    cl::Buffer seed_buf;
    output_buf = cl::Buffer(context, CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE, sizeof(TEST_DT),
                            &mext_o[0]);
    seed_buf = cl::Buffer(context, CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE,
                          sizeof(unsigned int), &mext_o[1]);

    for (int i = 0; i < opt_len; ++i) {
        for (int j = 0; j < st_len; ++j) {
            for (int k = 0; k < 1; ++k) {
                for (int l = 0; l < unly_len; ++l) {
                    for (int m = 0; m < d_len; ++m) {
                        for (int n = 0; n < r_len; ++n) {
                            for (int p = 0; p < vol_len; ++p) {
                                TEST_DT underlying = values[i].s;
                                TEST_DT riskFreeRate = values[i].r;
                                TEST_DT dividendYield = values[i].q;
                                TEST_DT volatility = values[i].v;

                                TEST_DT strike = values[i].strike;
                                int optionType = values[i].type;
                                TEST_DT barrier = values[i].barrier;
                                TEST_DT rebate = values[i].rebate;
                                xf::fintech::enums::BarrierType barrierType = values[i].barrierType;

                                TEST_DT timeLength = 1;

                                TEST_DT expectedVal = values[i].result;

                                std::vector<cl::Memory> ob_out;
                                ob_out.push_back(output_buf);

                                q.finish();
                                // launch kernel and calculate kernel execution time
                                std::cout << "kernel start------" << std::endl;
                                gettimeofday(&start_time, 0);
                                int j = 0;
                                kernel_Engine.setArg(j++, underlying);
                                kernel_Engine.setArg(j++, volatility);
                                kernel_Engine.setArg(j++, dividendYield);
                                kernel_Engine.setArg(j++, riskFreeRate);
                                kernel_Engine.setArg(j++, timeLength);
                                kernel_Engine.setArg(j++, strike);
                                kernel_Engine.setArg(j++, barrierType);
                                kernel_Engine.setArg(j++, optionType);
                                kernel_Engine.setArg(j++, seed_buf);
                                kernel_Engine.setArg(j++, output_buf);
                                kernel_Engine.setArg(j++, requiredTolerance);
                                kernel_Engine.setArg(j++, requiredSamples);
                                kernel_Engine.setArg(j++, timeSteps);

                                q.enqueueTask(kernel_Engine, nullptr, nullptr);

                                q.finish();
                                gettimeofday(&end_time, 0);
                                std::cout << "kernel end------" << std::endl;
                                std::cout << "Execution time " << tvdiff(&start_time, &end_time) << "us" << std::endl;
                                q.enqueueMigrateMemObjects(ob_out, 1, nullptr, nullptr);
                                q.finish();
                                TEST_DT diff = std::fabs(outputs[0] - goldens[idx]) / underlying;
                                if (diff > relative_err) {
                                    std::cout << "Output is wrong!" << std::endl;
                                    if (optionType)
                                        std::cout << "Put option:\n";
                                    else
                                        std::cout << "Call option:\n";
                                    std::cout << "   strike:              " << strike << "\n"
                                              << "   underlying:          " << underlying << "\n"
                                              << "   risk-free rate:      " << riskFreeRate << "\n"
                                              << "   volatility:          " << volatility << "\n"
                                              << "   dividend yield:      " << dividendYield << "\n"
                                              << "   maturity:            " << timeLength << "\n"
                                              << "   tolerance:           " << requiredTolerance << "\n"
                                              << "   requaried samples:   " << requiredSamples << "\n"
                                              << "   maximum samples:     " << maxSamples << "\n"
                                              << "   timesteps:           " << timeSteps << "\n"
                                              << "   golden:              " << goldens[idx] << "\n";
                                    std::cout << "Acutal value: " << outputs[idx]
                                              << ", Expected value: " << goldens[idx] << std::endl;
                                    std::cout << "error: " << diff << ", tolerance: " << relative_err << std::endl;
                                }
                                idx++;
                            }
                        }
                    }
                }
            }
        }
    }
#else

    bool optionType = optionTypes[0];
    TEST_DT strike = strikes[0];
    TEST_DT underlying = underlyings[0];
    TEST_DT dividendYield = dividendYields[0];
    TEST_DT riskFreeRate = riskFreeRates[0];
    TEST_DT volatility = volatilitys[0];

    mc_euro_k(underlying, volatility, dividendYield,
              riskFreeRate, // model parameter
              timeLength, strike,
              optionType, // option parameter
              seeds, outputs, requiredTolerance, requiredSamples, timeSteps);

    TEST_DT diff = std::fabs(outputs[0] - goldens[0]) / underlying;
    // comapre with golden result
    if (diff > relative_err) {
        std::cout << "Output is wrong!" << std::endl;
        if (optionType)
            std::cout << "Put option:\n";
        else
            std::cout << "Call option:\n";
        std::cout << "   strike:              " << strike << "\n"
                  << "   underlying:          " << underlying << "\n"
                  << "   risk-free rate:      " << riskFreeRate << "\n"
                  << "   volatility:          " << volatility << "\n"
                  << "   dividend yield:      " << dividendYield << "\n"
                  << "   maturity:            " << timeLength << "\n"
                  << "   tolerance:           " << requiredTolerance << "\n"
                  << "   requaried samples:   " << requiredSamples << "\n"
                  << "   maximum samples:     " << maxSamples << "\n"
                  << "   timesteps:           " << timeSteps << "\n"
                  << "   golden:              " << goldens[0] << "\n";
        std::cout << "Acutal value: " << outputs[0] << ", Expected value: " << goldens[idx] << std::endl;
        std::cout << "error: " << diff << ", tolerance: " << relative_err << std::endl;
    }
    return -1;

#endif
}
return 0;
}
