#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <chrono>
#include <memory>
#include <cmath>
#include <signal.h>

#include <unsupported/Eigen/SparseExtra>
#include <Eigen/Sparse>

#include <SmurffCpp/DataMatrices/Data.h>
#include <SmurffCpp/IO/MatrixIO.h>
#include <SmurffCpp/Utils/Distribution.h>

#include <SmurffCpp/Model.h>

#include <SmurffCpp/VMatrixIterator.hpp>
#include <SmurffCpp/ConstVMatrixIterator.hpp>
#include <SmurffCpp/VMatrixExprIterator.hpp>
#include <SmurffCpp/ConstVMatrixExprIterator.hpp>

#include <SmurffCpp/Utils/Error.h>

using namespace std;
using namespace Eigen;
using namespace smurff;

Model::Model()
   : m_num_latent(-1)
{
}

//num_latent - size of latent dimention
//dims - dimentions of train data
//init_model_type - samples initialization type
void Model::init(int num_latent, const PVec<>& dims, ModelInitTypes model_init_type)
{
   m_num_latent = num_latent;
   m_dims = std::unique_ptr<PVec<> >(new PVec<>(dims));

   for(size_t i = 0; i < dims.size(); ++i)
   {
      std::shared_ptr<Eigen::MatrixXd> sample(new Eigen::MatrixXd(m_num_latent, dims[i]));

      switch(model_init_type)
      {
      case ModelInitTypes::random:
         bmrandn(*sample);
         break;
      case ModelInitTypes::zero:
         sample->setZero();
         break;
      default:
         {
            THROWERROR("Invalid model init type");
         }
      }

      m_samples.push_back(sample);
   }

   Pcache.init(ArrayXd::Ones(m_num_latent));
}

double Model::predict(const PVec<> &pos) const
{
   auto &P = Pcache.local();
   P.setOnes();
   for(uint32_t d = 0; d < nmodes(); ++d)
      P *= col(d, pos.at(d)).array();
   return P.sum();
}

const Eigen::MatrixXd &Model::U(uint32_t f) const
{
   return *m_samples.at(f);
}

Eigen::MatrixXd &Model::U(uint32_t f)
{
   return *m_samples.at(f);
}

VMatrixIterator<Eigen::MatrixXd> Model::Vbegin(std::uint32_t mode)
{
   return VMatrixIterator<Eigen::MatrixXd>(shared_from_this(), mode, 0);
}

VMatrixIterator<Eigen::MatrixXd> Model::Vend()
{
   return VMatrixIterator<Eigen::MatrixXd>(m_samples.size());
}

ConstVMatrixIterator<Eigen::MatrixXd> Model::CVbegin(std::uint32_t mode) const
{
   return ConstVMatrixIterator<Eigen::MatrixXd>(shared_from_this(), mode, 0);
}

ConstVMatrixIterator<Eigen::MatrixXd> Model::CVend() const
{
   return ConstVMatrixIterator<Eigen::MatrixXd>(m_samples.size());
}

Eigen::MatrixXd::ConstColXpr Model::col(int f, int i) const
{
   return U(f).col(i);
}

std::uint64_t Model::nmodes() const
{
   return m_samples.size();
}

int Model::nlatent() const
{
   return m_num_latent;
}

int Model::nsamples() const
{
   return std::accumulate(m_samples.begin(), m_samples.end(), 0,
      [](const int &a, const std::shared_ptr<Eigen::MatrixXd> &b) { return a + b->cols(); });
}

const PVec<>& Model::getDims() const
{
   return *m_dims;
}

SubModel Model::full()
{
   std::shared_ptr<Model> this_model = shared_from_this();
   return this_model;
}

void Model::save(std::string prefix, std::string extension, std::vector<std::string>& paths)
{
   std::int32_t i = 0;
   for (auto U : m_samples)
   {
      std::string path = getModelFileName(prefix, extension, i++);
      smurff::matrix_io::eigen::write_matrix(path, *U);
      paths.push_back(path);
   }
}

void Model::restore(std::string prefix, std::string extension)
{
   int i = 0;
   for(auto U : m_samples)
   {
      smurff::matrix_io::eigen::read_matrix(prefix + "-U" + std::to_string(i++) + "-latents" + extension, *U);
   }
}

std::ostream& Model::info(std::ostream &os, std::string indent) const
{
   os << indent << "Num-latents: " << m_num_latent << "\n";
   return os;
}

std::ostream& Model::status(std::ostream &os, std::string indent) const
{
   Eigen::ArrayXd P = Eigen::ArrayXd::Ones(m_num_latent);
   
   for(std::uint64_t d = 0; d < nmodes(); ++d)
      P *= U(d).rowwise().norm().array();

   os << indent << "  Latent-wise norm: " << P.transpose() << "\n";
   return os;
}

std::string Model::getModelFileName(std::string prefix, std::string extension, std::int32_t index) const
{
   return prefix + "-U" + std::to_string(index) + "-latents" + extension;
}


Eigen::MatrixXd::BlockXpr SubModel::U(int f)
{
   return m_model->U(f).block(0, m_off.at(f), m_model->nlatent(), m_dims.at(f));
}

Eigen::MatrixXd::ConstBlockXpr SubModel::U(int f) const
{
   const Eigen::MatrixXd &u = m_model->U(f); //force const
   return u.block(0, m_off.at(f), m_model->nlatent(), m_dims.at(f));
}

VMatrixExprIterator<Eigen::MatrixXd::BlockXpr> SubModel::Vbegin(std::uint32_t mode)
{
   return VMatrixExprIterator<Eigen::MatrixXd::BlockXpr>(m_model, m_off, m_dims, mode, 0);
}

VMatrixExprIterator<Eigen::MatrixXd::BlockXpr> SubModel::Vend()
{
   return VMatrixExprIterator<Eigen::MatrixXd::BlockXpr>(m_model->nmodes());
}

ConstVMatrixExprIterator<Eigen::MatrixXd::ConstBlockXpr> SubModel::CVbegin(std::uint32_t mode) const
{
   return ConstVMatrixExprIterator<Eigen::MatrixXd::ConstBlockXpr>(m_model, m_off, m_dims, mode, 0);
}

ConstVMatrixExprIterator<Eigen::MatrixXd::ConstBlockXpr> SubModel::CVend() const
{
   return ConstVMatrixExprIterator<Eigen::MatrixXd::ConstBlockXpr>(m_model->nmodes());
}
