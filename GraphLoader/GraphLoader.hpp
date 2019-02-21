//
// Created by joonho on 18.02.19.
//
// This object runs tensorflow graph in cpp

#ifndef GRAPHLOADER_HPP
#define GRAPHLOADER_HPP

#include <string>
#include <vector>
#include <utility>
#include <memory>
#include <iostream>
#include <typeinfo>
#include <fstream>


#include <glog/logging.h>

#include <tensorflow/core/public/session.h>
#include <tensorflow/core/framework/tensor.h>
#include <tensorflow/core/framework/op_kernel.h>

template<typename Dtype>
class GraphLoader
{
 public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  //
  using MatrixXD = Eigen::Matrix<Dtype, Eigen::Dynamic, Eigen::Dynamic>;
  using VectorXD = Eigen::Matrix<Dtype, Eigen::Dynamic, 1>;
  using Tensor3D = Eigen::Tensor<Dtype, 3>;

 public:

  GraphLoader(tensorflow::GraphDef graphDef,
                          int n_threads = 0,
                          bool logDevicePlacment = false)
  {
    construct(graphDef, n_threads, logDevicePlacment);
  }

  GraphLoader(std::string pathToGraphDefProtobuf,
                          int n_threads = 0,
                          bool logDevicePlacment = false)
  {
    tensorflow::GraphDef graphDef;
    auto status = tensorflow::ReadBinaryProto(tensorflow::Env::Default(), pathToGraphDefProtobuf, &graphDef);
    if(!status.ok()){
      std::cout <<  status.ToString() << std::endl;
    }
    construct(graphDef, n_threads, logDevicePlacment);
  }

  ~GraphLoader()
  {
    delete session;
  }

  // mat to mat
  inline void run(const std::vector<std::pair<std::string, MatrixXD>> &inputs,
                  const std::vector<std::string> &outputTensorNames,
                  const std::vector<std::string> &targetNodeNames,
                  std::vector<MatrixXD> &outputs)
  {
    // Create local tensorflow::Tensor buffer from provided Eigen::Matrix
    std::vector<std::pair<std::string, tensorflow::Tensor> > namedInputTensorFlowTensors;
    namedEigenMatricesToNamedTFTensors(inputs, namedInputTensorFlowTensors);

    // Local output tensorflow::Tensor buffer
    std::vector<tensorflow::Tensor> outputTensorFlowTensors;

    // Run the desired operations
    auto status = session->Run(namedInputTensorFlowTensors, outputTensorNames, targetNodeNames, &outputTensorFlowTensors);
    if(!status.ok()){
      std::cout <<  status.ToString() << std::endl;
    }

    // Convert tensorflow::Tensor to Eigen::Matrix
    tfTensorsToEigenMatrices(outputTensorFlowTensors, outputs);
  }

  // mat no output (just run operations)
  inline void run(const std::vector<std::pair<std::string, MatrixXD>> &inputs,
                  const std::vector<std::string> &outputTensorNames,
                  const  std::vector<std::string> &targetNodeNames)
  {
    // Create local tensorflow::Tensor buffer from provided Eigen::Matrix
    std::vector<std::pair<std::string, tensorflow::Tensor> > namedInputTensorFlowTensors;
    namedEigenMatricesToNamedTFTensors(inputs, namedInputTensorFlowTensors);

    // Local output tensorflow::Tensor buffer
    std::vector<tensorflow::Tensor> outputTensorFlowTensors;

    // Run the desired operations
    auto status = session->Run(namedInputTensorFlowTensors, outputTensorNames, targetNodeNames, &outputTensorFlowTensors);
    if(!status.ok()){
      std::cout <<  status.ToString() << std::endl;
    }
  }

  // tensor to tensor
  inline void run(const std::vector<std::pair<std::string, tensorflow::Tensor>> &inputs,
                  const std::vector<std::string> &outputTensorNames,
                  const  std::vector<std::string> &targetNodeNames,
                  std::vector<tensorflow::Tensor> &outputs)
  {
    // Local output tensorflow::Tensor buffer
    std::vector<tensorflow::Tensor> outputTensorFlowTensors;

    // Run the desired operations
    auto status = session->Run(inputs, outputTensorNames, targetNodeNames, &outputTensorFlowTensors);
    if(!status.ok()){
      std::cout <<  status.ToString() << std::endl;
    }

    // Output w/o conversion
    outputs = outputTensorFlowTensors;
  }

  // tensor to mat
  inline void run(const std::vector<std::pair<std::string, tensorflow::Tensor>> &inputs,
                  const std::vector<std::string> &outputTensorNames,
                  const  std::vector<std::string> &targetNodeNames,
                  std::vector<MatrixXD> &outputs)
  {
  // Local output tensorflow::Tensor buffer
    std::vector<tensorflow::Tensor> outputTensorFlowTensors;

    // Run the desired operations
    auto status = session->Run(inputs, outputTensorNames, targetNodeNames, &outputTensorFlowTensors);
    if(!status.ok()){
      std::cout <<  status.ToString() << std::endl;
    }

    // Convert tensorflow::Tensor to Eigen::Matrix
    tfTensorsToEigenMatrices(outputTensorFlowTensors, outputs);
  }

  // tensor no output (just run a node)
  inline void run(const std::vector<std::pair<std::string, tensorflow::Tensor>> &inputs,
                  const std::vector<std::string> &outputTensorNames,
                  const  std::vector<std::string> &targetNodeNames)
  {
    // Local output tensorflow::Tensor buffer
    std::vector<tensorflow::Tensor> outputTensorFlowTensors;

    // Run the desired operations
    auto status = session->Run(inputs, outputTensorNames, targetNodeNames, &outputTensorFlowTensors);
    if(!status.ok()){
      std::cout <<  status.ToString() << std::endl;
    }
  }

  inline static void eigenMatrixToTFTensor(const MatrixXD &matrix, tensorflow::Tensor &tensor) {
    LOG_IF(FATAL, tensor.shape().dims() > 2)
    << "copyEigenMatrixToTensorFlowTensor requires rank 2 tensors (matrices).";
    int rows = std::max(int(tensor.shape().dim_size(0)), 1);
    int cols = std::max(int(tensor.shape().dim_size(1)), 1);
    LOG_IF(FATAL, rows != matrix.cols() || cols != matrix.rows())
    << "dimensions don't match. Eigen matrix and Tensorflow tensor should be transpose of each other Eigen is colmajor and Tensorflow is rowmajor"
    << std::endl
    << "(" << rows << ", " << cols << ") vs (" << matrix.rows() << ", " << matrix.cols() << ")";
    memcpy(tensor.flat<Dtype>().data(), matrix.data(), sizeof(Dtype) * tensor.shape().num_elements());
  }

  inline static void tfTensorToEigenMatrix(const tensorflow::Tensor &tensor, MatrixXD &matrix) {
    LOG_IF(FATAL, tensor.shape().dims() > 2) << "copyTensorFlowTensorToEigenMatrix requires rank 2 tensors (matrices)";
    int rows = std::max(int(tensor.shape().dim_size(0)), 1);
    int cols = std::max(int(tensor.shape().dim_size(1)), 1);
    if (tensor.shape().dims() == 2) matrix.resize(cols, rows);
    else matrix.resize(1, 1);
    memcpy(matrix.data(), tensor.flat<Dtype>().data(), sizeof(Dtype) * tensor.shape().num_elements());
  }

   static void tfTensorsToEigenMatrices(const std::vector<tensorflow::Tensor> &input, std::vector<MatrixXD> &output) {
    output.clear();
    for (auto &element : input) {
      MatrixXD matrix;
      tfTensorToEigenMatrix(element, matrix);
      output.push_back(matrix);
    }
  }

  static void namedEigenMatricesToNamedTFTensors(
      const std::vector<std::pair<std::string, MatrixXD>> &input,
      std::vector<std::pair<std::string, tensorflow::Tensor>> &output) {
    output.clear();
    for (auto &element : input) {
      tensorflow::Tensor tensor(getTensorFlowDataType(),
                                tensorflow::TensorShape({element.second.cols(), element.second.rows()}));
      eigenMatrixToTFTensor(element.second, tensor);
      output.push_back(std::pair<std::string, tensorflow::Tensor>(element.first, tensor));
    }
  }

  static tensorflow::DataType getTensorFlowDataType() {
    if (typeid(Dtype) == typeid(float))
      return tensorflow::DataType::DT_FLOAT;
    else if (typeid(Dtype) == typeid(double))
      return tensorflow::DataType::DT_DOUBLE;
    LOG(FATAL) << "TensorFlowNeuralNetwork is only implemented for float and double";
  }

  void setGraphDef(const tensorflow::GraphDef graphDef) {
    graphDef_ = graphDef;
    auto status = session->Close();
    LOG_IF(FATAL, !status.ok()) << status.ToString();
    tensorflow::NewSession(tensorflow::SessionOptions(), &session);
    status = session->Create(graphDef);
    LOG_IF(FATAL, !status.ok()) << status.ToString();
    updateParams();
  }

  tensorflow::GraphDef getGraphDef() const {
    return graphDef_;
  }

  int getGlobalStep(){
    std::vector<tensorflow::Tensor> globalStep;
    auto status = this->session->Run({}, {"global_step"}, {}, &globalStep);
    LOG_IF(FATAL, !status.ok()) << status.ToString();
    return globalStep[0].scalar<int>()();
  }

  virtual void loadParam(const std::string &fileName, const std::string &scope) {
    const static Eigen::IOFormat CSVFormat(20, Eigen::DontAlignCols, ", ", "\n");
    int size = getParamsSize(scope);
    std::cout << size << std::endl;
    VectorXD param(size);

    std::stringstream parameterFileName;
    std::ifstream indata;
    indata.open(fileName);

    if(!indata.is_open()){
      std::cout << "Parameter file " << fileName << " could not be opened" << std::endl;
    }


    std::string line;
    getline(indata, line);
    std::stringstream lineStream(line);
    std::string cell;
    int paramSize = 0;
    while (std::getline(lineStream, cell, ','))
      param(paramSize++) = std::stof(cell);
    LOG_IF(FATAL, size != paramSize) << "Parameter sizes don't match" << size << ", " << paramSize;

    setParams(param, scope);
  }

  void setParams(const Eigen::Matrix<Dtype, -1, 1> &policyParams, const std::string &scope) {
    this->run({{scope + "/params_assign_placeholder", policyParams}},
                   {},
                   {scope + "/params_assign_op"});
  }

  size_t getParamsSize(const std::string &scope) {
    std::vector<tensorflow::Tensor> paramV;
    auto status = this->session->Run({}, {scope + "/sizeOfParams"}, {}, &paramV);
    LOG_IF(FATAL, !status.ok()) << status.ToString();
    return paramV[0].scalar<int>()();
  }

 protected:
  /*
   * Method used in the different constructors
   * @graphDef: tensorflow::GraphDef to be used
   * @n_inter_op_parallelism_threads: The execution of an individual op (for some op types) can be parallelized on a pool of intra_op_parallelism_threads.
   * @logDevicePlacement: If true, Upon reading the GraphDef, TensorFlow shows where (which CPU or GPU) a particular op is assigned
   */
  void construct(tensorflow::GraphDef graphDef,
                 int n_inter_op_parallelism_threads = 0,
                 bool logDevicePlacment = false) {
    tensorflow::ConfigProto configProto;
    configProto.mutable_gpu_options()->set_allow_growth(true);
    configProto.set_allow_soft_placement(true);
    configProto.set_log_device_placement(logDevicePlacment);
    if (n_inter_op_parallelism_threads > 0)
      configProto.set_inter_op_parallelism_threads(n_inter_op_parallelism_threads);
    tensorflow::SessionOptions sessionOptions;
    sessionOptions.env = tensorflow::Env::Default();
    sessionOptions.config = configProto;
    tensorflow::NewSession(sessionOptions, &session);
    graphDef_ = graphDef;
    // Add the graph to the session
    auto status = session->Create(graphDef);
    LOG_IF(FATAL, !status.ok()) << status.ToString();
    // Initialize internals
    for (int i = 0; i < graphDef_.node_size(); ++i)
      numericsOpNames_.push_back(graphDef_.node(i).name());
    // Initialize all variables
    updateParams();
  }

 private:
  void updateParams()
  {
    // Run initialization ops
    auto status = session->Run({}, {}, {"initializeAllVariables"}, {});
    LOG_IF(FATAL, !status.ok()) << status.ToString();
  }

 public:
  tensorflow::Session *session;

 protected:
  tensorflow::GraphDef graphDef_;
  std::vector<std::string> numericsOpNames_;
};

#endif //GRAPHLOADER_HPP
