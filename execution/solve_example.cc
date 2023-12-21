// Copyright 2022 DeepMind Technologies Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// A simple utility that prints the names of the problems in a dataset. If
// provided multiple filenames as arguments, these are read sequentially.

#include <fcntl.h>

#include <functional>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>
#include <fstream>

#include "absl/flags/parse.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "contest_problem.pb.h"
#include "execution/py_locations.h"
#include "execution/py_tester_sandboxer.h"
#include "execution/status_macros.h"
#include "execution/tester_sandboxer.h"
#include "riegeli/bytes/fd_reader.h"
#include "riegeli/records/record_reader.h"
#include "nlohmann/json.hpp"

ABSL_FLAG(std::string, valid_path, "", "Path to validation dataset.");
ABSL_FLAG(std::string, problem_no, "", "start index of the problem.");

namespace deepmind::code_contests {
namespace {

std::vector<absl::string_view> GetInputs(const ContestProblem& problem,
                                         int max_size) {
  std::vector<absl::string_view> inputs;
  for (const auto& test : problem.public_tests()) {
    inputs.push_back(test.input());
  }
  for (const auto& test : problem.private_tests()) {
    inputs.push_back(test.input());
  }
  for (const auto& test : problem.generated_tests()) {
    inputs.push_back(test.input());
  }
  // inputs.resize(max_size);
  return inputs;
}

std::vector<absl::string_view> GetOutputs(const ContestProblem& problem,
                                          int max_size) {
  std::vector<absl::string_view> outputs;
  for (const auto& test : problem.public_tests()) {
    outputs.push_back(test.output());
  }
  for (const auto& test : problem.private_tests()) {
    outputs.push_back(test.output());
  }
  for (const auto& test : problem.generated_tests()) {
    outputs.push_back(test.output());
  }
  // outputs.resize(max_size);
  return outputs;
}

void ReportResults(const MultiTestResult& multi_result) {
  std::cout << "Compilation "
            << (multi_result.compilation_result.program_status ==
                        ProgramStatus::kSuccess
                    ? "succeeded"
                    : "failed")
            << "\n";
  int i = 0;
  for (const auto& test_result : multi_result.test_results) {
    if (!test_result.passed.has_value()) {
      std::cout << "Test " << i << " did not run.\n";
    } else if (*test_result.passed) {
      std::cout << "Test " << i << " passed.\n";
    } else {
      std::cout << "Test " << i << " failed.\n";
    }
    ++i;
  }
}

absl::Status SolveGregorAndCryptography(
  const absl::string_view valid_filename, int problem_no) {

  riegeli::RecordReader<riegeli::FdReader<>> reader(
     std::forward_as_tuple(valid_filename));
  ContestProblem problem;

  std::string json_file_name(valid_filename);
  json_file_name.replace(json_file_name.find(".json"), 5, "_perfed.json");

  std::string filename(valid_filename);
  std::cout << "Start index: " << problem_no << std::endl;

  while (reader.ReadRecord(problem)) {
    nlohmann::json json_data;
    json_data["name"] = problem.name(); 
    json_data["id"] = problem_no;
    std::cout << "Start perf problem: " << problem_no << "-" << problem.name() << std::endl;
    const std::vector<absl::string_view> inputs =
        GetInputs(problem, 10);
    const std::vector<absl::string_view> outputs =
        GetOutputs(problem, 10);

    json_data["number"] = problem.solutions_size();

    Py3TesterSandboxer tester(Py3InterpreterPath(), Py3LibraryPaths());
    Py2TesterSandboxer tester_py2(Py2InterpreterPath(), Py2LibraryPaths());
    TestOptions options;
    options.num_threads = 40; 
    options.stop_on_first_failure = true;

    std::vector<double> times;
    for (int i=0; i<problem.solutions_size(); ++i) {
      // we only care about python solutions
      MultiTestResult *multi_result = nullptr;
      if (problem.solutions(i).language() != 1 || problem.solutions(i).language() != 3) {
        times.push_back(0.0);
        continue;
      // python2
      } else if (problem.solutions(i).language() == 1) {
        ASSIGN_OR_RETURN(MultiTestResult result,
                        tester_py2.Test(problem.solutions(i).solution(), inputs, options, outputs));
        multi_result = &result;
      // python3
      } else { 
        ASSIGN_OR_RETURN(MultiTestResult result,
                        tester.Test(problem.solutions(i).solution(), inputs, options, outputs));
        multi_result = &result;
      }

      // Get the total time
      double total_time = 0.0;
      for (const auto& test_result : multi_result->test_results) {
        if (*test_result.passed) {
          double execution_duration = static_cast<double>(ToInt64Nanoseconds(test_result.execution_duration)) / 1e9;
          total_time += execution_duration;
        }
        else {
          total_time = 0;
          break;
        }
      }
      times.push_back(total_time);
    }

    json_data["times"] = times;
    std::ofstream out(json_file_name, std::ios_base::app);
    out << json_data.dump() << std::endl;
    out.close();
    std::cout << "finished!" << std::endl;
  }
  return absl::OkStatus();
}

}  // namespace
}  // namespace deepmind::code_contests

int main(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);
  const std::string filename = absl::GetFlag(FLAGS_valid_path);
  const int problem_no = std::stoi(absl::GetFlag(FLAGS_problem_no));
  if (filename.empty()) {
    std::cerr << "The flag `valid_path` was empty and it should not be, please "
                 "pass `--valid_path=...` "
              << std::endl;
  } else {
    absl::Status status =
        deepmind::code_contests::SolveGregorAndCryptography(filename, problem_no);
    if (!status.ok()) {
      std::cerr << "Failed: " << status.message() << std::endl;
    }
  }
}
