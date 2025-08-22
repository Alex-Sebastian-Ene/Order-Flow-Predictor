"""Neural network implementation for order flow prediction."""

#pragma once

#include <vector>
#include <memory>

namespace order_flow {

class NeuralNetwork {
public:
    /**
     * @brief Construct a new Neural Network object
     * @param input_size Number of input features
     * @param hidden_sizes Vector of hidden layer sizes
     * @param output_size Number of output neurons
     */
    NeuralNetwork(int input_size, 
                  const std::vector<int>& hidden_sizes, 
                  int output_size);
    
    /**
     * @brief Forward pass through the network
     * @param input Input feature vector
     * @return Output predictions
     */
    std::vector<double> forward(const std::vector<double>& input);
    
    /**
     * @brief Train the network using backpropagation
     * @param inputs Training input data
     * @param targets Training targets
     * @param epochs Number of training epochs
     * @param learning_rate Learning rate for gradient descent
     */
    void train(const std::vector<std::vector<double>>& inputs,
               const std::vector<std::vector<double>>& targets,
               int epochs,
               double learning_rate = 0.001);
    
    /**
     * @brief Save model weights to file
     * @param filename Path to save weights
     */
    void save_weights(const std::string& filename) const;
    
    /**
     * @brief Load model weights from file
     * @param filename Path to load weights from
     */
    void load_weights(const std::string& filename);

private:
    struct Layer {
        std::vector<std::vector<double>> weights;
        std::vector<double> biases;
        std::vector<double> activations;
    };
    
    std::vector<Layer> layers_;
    int input_size_;
    int output_size_;
    
    // Activation functions
    double sigmoid(double x) const;
    double sigmoid_derivative(double x) const;
    
    // Helper functions
    void initialize_weights();
    void backward_pass(const std::vector<double>& target);
    void update_weights(double learning_rate);
};

} // namespace order_flow
