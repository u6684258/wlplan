#include "../../../include/feature_generator/feature_generators/wl.hpp"

#include "../../../include/graph_generator/graph_generator_factory.hpp"
#include "../../../include/utils/exceptions.hpp"
#include "../../../include/utils/nlohmann/json.hpp"

#include <fstream>
#include <queue>
#include <set>
#include <sstream>

using json = nlohmann::json;

namespace wlplan {
  namespace feature_generator {
    WLFeatures::WLFeatures(const std::string wl_name,
                           const planning::Domain &domain,
                           std::string graph_representation,
                           int iterations,
                           std::string pruning,
                           bool multiset_hash)
        : Features(wl_name, domain, graph_representation, iterations, pruning, multiset_hash) {}

    WLFeatures::WLFeatures(const planning::Domain &domain,
                           std::string graph_representation,
                           int iterations,
                           std::string pruning,
                           bool multiset_hash)
        : Features("wl", domain, graph_representation, iterations, pruning, multiset_hash) {}

    WLFeatures::WLFeatures(const std::string &filename) : Features(filename) {}

    WLFeatures::WLFeatures(const std::string &filename, bool quiet) : Features(filename, quiet) {}

    int WLFeatures::get_n_features() const { return get_n_colours(); }

    void WLFeatures::refine(const std::shared_ptr<graph_generator::Graph> &graph,
                            std::set<int> &nodes,
                            Embedding &colours,
                            int iteration,
                            int data_index) {
      // memory for storing string and hashed int representation of colours
      std::vector<int> new_colour;
      int new_colour_compressed;

      Embedding new_colours;
      for (int i = 0; i < (int) colours.size(); i++) {
        new_colours.insert({i, UNSEEN_COLOUR});
      }
      std::vector<int> nodes_to_discard;

      for (const int u : nodes) {
        // skip unseen colours
        int current_colour = colours[u];
        if (current_colour == UNSEEN_COLOUR) {
          new_colour_compressed = UNSEEN_COLOUR;
          nodes_to_discard.push_back(u);
          goto end_of_iteration;
        }
        neighbour_container->clear();

        for (const auto &edge : graph->edges[u]) {
          // skip unseen colours
          int neighbour_colour = colours[edge.second];
          if (neighbour_colour == UNSEEN_COLOUR) {
            new_colour_compressed = UNSEEN_COLOUR;
            nodes_to_discard.push_back(u);
            goto end_of_iteration;
          }

          // add sorted neighbour (colour, edge_label) pair
          neighbour_container->insert(neighbour_colour, edge.first);
        }

        // add current colour and sorted neighbours into sorted colour key
        new_colour = neighbour_container->to_vector();
        new_colour.push_back(current_colour);

        // hash seen colours
        new_colour_compressed = get_colour_hash(new_colour, iteration, data_index);

      end_of_iteration:
        new_colours[u] = new_colour_compressed;
      }

      // discard nodes
      for (const int u : nodes_to_discard) {
        nodes.erase(u);
      }

      colours = std::move(new_colours);
    }

    void WLFeatures::refine_fast(const std::shared_ptr<graph_generator::Graph> &graph,
                                 Embedding &colours,
                                 int iteration) {
      // memory for storing string and hashed int representation of colours
      std::vector<int> new_colour;
      Embedding new_colours;
      for (int i = 0; i < (int) colours.size(); i++) {
        new_colours.insert({i, UNSEEN_COLOUR});
      }

      for (size_t u = 0; u < colours.size(); u++) {
        neighbour_container->clear_init(graph->edges[u].size());

        for (const auto &edge : graph->edges[u]) {
          // add sorted neighbour (colour, edge_label) pair
          neighbour_container->insert(colours[edge.second], edge.first);
        }

        // add current colour and sorted neighbours into sorted colour key
        new_colour = neighbour_container->to_vector();
        new_colour.push_back(colours[u]);

        // hash
        new_colours[u] = get_colour_hash_fast(new_colour, iteration);
      }

      colours = std::move(new_colours);
    }

    void WLFeatures::collect_impl(const std::vector<graph_generator::Graph> &graphs) {
      // Intermediate graph colours during WL
      // It could be more optimal to use map<int, int> for graph colours, with UNSEEN_COLOUR
      // nodes not showing up in the map. However, this would make the code more complex.
      std::vector<Embedding> graph_colours;
      graph_colours.reserve(graphs.size());

      // init colours
      log_iteration(0);
      for (size_t graph_i = 0; graph_i < graphs.size(); graph_i++) {
        const auto graph = std::make_shared<graph_generator::Graph>(graphs[graph_i]);
        int n_nodes = graph->nodes.size();

        Embedding colours;
        for (int i = 0; i < n_nodes; i++) {
          colours.insert({i, 0});
        }
        for (int node_i = 0; node_i < n_nodes; node_i++) {
          int col = get_colour_hash({graph->nodes[node_i]}, 0, graph_i);
          colours[node_i] = col;
        }
        graph_colours.push_back(colours);
      }

      // main WL loop
      for (int itr = 1; itr < iterations + 1; itr++) {
        log_iteration(itr);
        for (size_t graph_i = 0; graph_i < graphs.size(); graph_i++) {
          const auto graph = std::make_shared<graph_generator::Graph>(graphs[graph_i]);
          std::set<int> nodes = graph->get_nodes_set();
          refine(graph, nodes, graph_colours[graph_i], itr, graph_i);
        }

        // layer pruning
        prune_this_iteration(itr, graph_colours);
      }
    }
	
    void WLFeatures::collect_impl(const std::vector<data::ProblemDataset> &data) {
      // Intermediate graph colours during WL
      // It could be more optimal to use map<int, int> for graph colours, with UNSEEN_COLOUR
      // nodes not showing up in the map. However, this would make the code more complex.
      std::vector<Embedding> graph_colours;
      size_t ret = 0;
      for (const auto &problem_states : data) {
        ret += problem_states.states.size();
      }
      graph_colours.reserve(ret);
      int data_index = 0;
      // init colours
      log_iteration(0);
      for (const auto &problem_states : data) {
      const auto &problem = problem_states.problem;
      const auto &states = problem_states.states;
        graph_generator->set_problem(problem);
        std::string p_string = problem.to_string();
        for (const planning::State &state : states) {
          const auto graph = graph_generator->to_graph(state);
          int n_nodes = graph->nodes.size();

          Embedding colours;
          for (int i = 0; i < n_nodes; i++) {
            colours.insert({i, 0});
          }
          for (int node_i = 0; node_i < n_nodes; node_i++) {
            int col = get_colour_hash({graph->nodes[node_i]}, 0, data_index);
            colours[node_i] = col;
          }
          graph_colours.push_back(colours);
		  data_index++;
        }
	  }
      // main WL loop
      for (int itr = 1; itr < iterations + 1; itr++) {
		    data_index = 0;
        log_iteration(itr);
        for (const auto &problem_states : data) {
          const auto &problem = problem_states.problem;
          const auto &states = problem_states.states;
          graph_generator->set_problem(problem);
		  for (const planning::State &state : states) {
            const auto graph = graph_generator->to_graph(state);
          std::set<int> nodes = graph->get_nodes_set();
          refine(graph, nodes, graph_colours[data_index], itr, data_index);
		data_index++;
        }
	}
        // layer pruning
        prune_this_iteration(itr, graph_colours);
      }
    }


    Embedding WLFeatures::embed_impl(const std::shared_ptr<graph_generator::Graph> &graph) {
      /* 1. Initialise embedding before pruning, and set up memory */
      Embedding x0;
      for (int i = 0; i < get_n_colours(); i++) {
        x0.insert({i, 0});
      }
      int n_nodes = graph->nodes.size();
      Embedding colours;
      for (int i = 0; i < n_nodes; i++) {
        colours.insert({i, 0});
      }
      std::set<int> nodes = graph->get_nodes_set();

      /* 2. Compute initial colours */
      for (const int node_i : nodes) {
        int col = get_colour_hash({graph->nodes[node_i]}, 0);
        colours[node_i] = col;
        add_colour_to_x(col, 0, x0);
      }

      /* 3. Main WL loop */
      for (int itr = 1; itr < iterations + 1; itr++) {
        refine(graph, nodes, colours, itr);
        for (auto pair : colours) {
          add_colour_to_x(pair.second, itr, x0);
        }
      }

      return x0;
    }
  }  // namespace feature_generator
}  // namespace wlplan
