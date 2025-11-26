#include "../../include/feature_generator/maxsat.hpp"

#include "../../include/utils/exceptions.hpp"

#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <random>
#ifndef NOPYTHON
#include <pybind11/embed.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/typing.h>
namespace py = pybind11;
#endif

using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

namespace wlplan {
  namespace feature_generator {
    MaxSatClause::MaxSatClause(const std::vector<int> &variables,
                               const std::vector<bool> &negated,
                               const int weight,
                               const bool hard)
        : variables(variables), negated(negated), weight(weight), hard(hard) {
      if (hard && weight != 0) {
        throw std::runtime_error("Hard MaxSAT clauses should have 0 weight!");
      } else if (!hard && weight < 1) {
        throw std::runtime_error("Soft MaxSAT clauses should have weight >= 1!");
      }
      if (variables.size() != negated.size()) {
        throw std::runtime_error(
            "Variables and negated in MaxSatClause should have the same size!");
      }
      for (size_t i = 0; i < variables.size(); i++) {
        if (variables[i] < 1) {
          throw std::runtime_error("Variables in MaxSatClause should be strictly positive!");
        }
      }
    }

  std::string MaxSatClause::to_string(std::string hard_header) const {
    std::string ret = "";
    if (hard) {
      ret += hard_header + " ";
    } else {
      ret += std::to_string(weight) + " ";
    }
    for (int i = 0; i < size(); i++) {
      if (negated[i]) {
        ret += "-";
      }
      ret += std::to_string(variables[i]) + " ";
    }
    ret += "0";
    return ret;
  }

  MaxSatProblem::MaxSatProblem(const std::vector<MaxSatClause> &clauses) : clauses(clauses) {
    for (const MaxSatClause &clause : clauses) {
      for (int variable : clause.variables) {
        variables.insert(variable);
      }
    }
  }

  std::string MaxSatProblem::to_string() {
    // older wcnf version
    // https://maxsat-evaluations.github.io/2021/rules.html#input

    uint64_t sum_variable = 0;
    uint64_t max_variable = 0;
    for (const MaxSatClause &clause : clauses) {
      sum_variable = clause.weight + sum_variable;
      max_variable = std::max(max_variable, (uint64_t) clause.weight);
    }
    sum_variable += 1;  // add one for the top variable
    if (sum_variable < max_variable) {
      throw std::runtime_error("Sum of weights is less than max weight!");
    }
    std::string top_value = std::to_string(sum_variable);

    std::string ret = "p wcnf ";
    ret += std::to_string(sum_variable) + " ";
    ret += std::to_string(clauses.size()) + " ";
    ret += top_value + "\n";
    std::cout << "  Variables: " << get_n_variables() << std::endl;
    std::cout << "  Clauses: " << clauses.size() << std::endl;
    std::cout << "  Max variable weights (Top): " << sum_variable << std::endl;
    for (const MaxSatClause &clause : clauses) {
      ret += clause.to_string(top_value) + "\n";
    }
    return ret;

    // // 2022 wcnf version
    // // https://maxsat-evaluations.github.io/2022/rules.html#input
    // std::string ret = "";
    // for (const MaxSatClause &clause : clauses) {
    //   ret += clause.to_string("h") + "\n";
    // }
    // return ret;
  }

  std::map<int, int> MaxSatProblem::call_solver() {
//#ifndef NOPYTHON
//
//    std::string maxsat_wcnf_string = to_string();
//
//#ifdef DEBUGMODE
//    std::cout << maxsat_wcnf_string << std::endl;
//#endif
//
//    py::object pysat_rc2 = py::module::import("pysat.examples.rc2").attr("RC2");
//    py::object pysat_wcnf = py::module::import("pysat.formula").attr("WCNF");
//    py::dict kwargs;
//    kwargs["from_string"] = maxsat_wcnf_string;
//    py::object wncf = pysat_wcnf(**kwargs);
//    py::object rc2 = pysat_rc2(wncf, py::arg("solver") = "g4");
//
//    std::cout << "Solving MaxSAT with RC2." << std::endl;
//    auto t1 = high_resolution_clock::now();
//    py::list pylist_solution = rc2.attr("compute")();
//    auto t2 = high_resolution_clock::now();
//    duration<double, std::milli> ms_double = t2 - t1;
//    py::int_ cost = rc2.attr("cost");
//    std::cout << "MaxSAT solved!" << std::endl;
//    std::cout << "  Solving time: " << ms_double.count() / 1000 << "s\n";
//    std::cout << "  Solution cost: " << cost << std::endl;
//
//    std::vector<int> solution_vector = py::cast<std::vector<int>>(pylist_solution);
//    std::map<int, int> solution;
//    for (const int value : solution_vector) {
//      int variable = std::abs(value);
//      if (variables.count(variable) == 0) {
//        continue;
//      }
//      solution[variable] = (value > 0);
//    }
//    return solution;
//#else
//    throw std::runtime_error("MaxSAT feature pruning is not supported in the C++ interface.");
//#endif
// Generate the WCNF string
    std::string maxsat_wcnf_string = to_string();

    // Write the WCNF string to a temporary file
    // Generate a random number
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);

    // Combine the temporary directory path with a random number
    const std::string temp_wcnf_file = "./temp_" + std::to_string(dis(gen)) + ".wcnf";
    const std::string temp_output_file = "./temp_output_" + std::to_string(dis(gen)) + ".txt";

    {
      std::ofstream wcnf_file(temp_wcnf_file);
      if (!wcnf_file) {
        throw std::runtime_error("Failed to create temp.wcnf file!");
      }
      wcnf_file << maxsat_wcnf_string;
    }

    // Call the external MaxSAT solver
    std::string command = "./uwrmaxsat " + temp_wcnf_file + " > " + temp_output_file;
    int ret_code = std::system(command.c_str());

    // Read the output file
    std::ifstream output_file(temp_output_file);
    if (!output_file) {
      throw std::runtime_error("Failed to open temp_output.txt file!");
    }

    // Delete the temp.wcnf file
    if (std::remove(temp_wcnf_file.c_str()) != 0) {
      throw std::runtime_error("Failed to delete temp.wcnf file!");
    }

    std::string line;
    std::string solution_line;
    bool optimum_found = false;

    while (std::getline(output_file, line)) {
      if (line.find("Optimal solution: ") != std::string::npos) {
        // This line contains the optimal solution cost
//        std::cout << "Optimal solution cost found in solver output: " << line << std::endl;
        continue;
      }
      if (line == "s OPTIMUM FOUND") {
        optimum_found = true;
        std::cout << "optimum found" << std::endl;
      } else if (optimum_found) {
        solution_line = line.substr(1);  // Skip the first character 'v'
        std::cout << "Solver output: " << line << std::endl;
        break;
      }
    }
    output_file.close();

    if (!optimum_found) {
      throw std::runtime_error("No solution found: 's OPTIMUM FOUND' not present in solver output!");
    }

    // Delete the temp_output.txt file
    if (std::remove(temp_output_file.c_str()) != 0) {
      throw std::runtime_error("Failed to delete temp_output.txt file!");
    }

    // Return the solution line as a string
    std::istringstream solution_stream(solution_line);
    std::map<int, int> solution;
    int value;
    while (solution_stream >> value) {
      int variable = std::abs(value);
      if (variables.count(variable) == 0) {
        continue;
      }
      solution[variable] = (value > 0);
//      std::cout << "Variable " << variable << " is " << value << std::endl;
    }
    std::cout << "MaxSAT solved!" << std::endl;
    //    std::cout << "  Solving time: " << ms_double.count() / 1000 << "s\n";
    //    std::cout << "  Solution cost: " << cost << std::endl;

    return solution;
  }

  std::map<int, int> MaxSatProblem::solve() {
//#ifndef NOPYTHON
//    if (Py_IsInitialized() == 0) {
//      // interpreter is not running
//      py::scoped_interpreter guard{};
//      return call_solver();
//    } else {
//      // interpreter is running (e.g. Python calling wlplan)
//      return call_solver();
//    }
//#else
//    throw std::runtime_error("MaxSAT feature pruning is not supported in the C++ interface.");
//#endif
    return call_solver();
  }

  }  // namespace feature_generator
}  // namespace wlplan
