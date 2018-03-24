#include "SpikeAndSlabPrior.h"

#include <SmurffCpp/IO/MatrixIO.h>
#include <SmurffCpp/IO/GenericIO.h>
#include <SmurffCpp/Utils/Error.h>

using namespace smurff;
using namespace Eigen;

SpikeAndSlabPrior::SpikeAndSlabPrior(std::shared_ptr<BaseSession> session, uint32_t mode)
   : NormalOnePrior(session, mode, "SpikeAndSlabPrior")
{

}

void SpikeAndSlabPrior::init()
{
   NormalOnePrior::init();

   const int K = num_latent();
   const int D = num_cols();
   const int nview = data()->nview(m_mode);
   
   THROWERROR_ASSERT(D > 0);

   Zcol.init(MatrixXd::Zero(K,nview));
   W2col.init(MatrixXd::Zero(K,nview));

   //-- prior params
   alpha = ArrayXXd::Ones(K,nview);
   Zkeep = ArrayXXd::Constant(K, nview, D);
   r = ArrayXXd::Constant(K,nview,.5);

   initLogRLogAlpha();
}

void SpikeAndSlabPrior::update_prior()
{
   const int nview = data()->nview(m_mode);
   
   Zkeep = Zcol.combine();
   auto W2c = W2col.combine();

   // update hyper params (alpha and r) (per view)
   for(int v=0; v<nview; ++v) {
       const int D = data()->view_size(m_mode, v);
       r.col(v) = ( Zkeep.col(v).array() + prior_beta ) / ( D + prior_beta * D ) ;
       auto ww = W2c.col(v).array() / 2 + prior_beta_0;
       auto tmpz = Zkeep.col(v).array() / 2 + prior_alpha_0 ;
       alpha.col(v) = tmpz.binaryExpr(ww, [](double a, double b)->double {
               return rgamma(a, 1/b) + 1e-7;
       });
   }

   initLogRLogAlpha();
}

void SpikeAndSlabPrior::initLogRLogAlpha()
{
   const int K = num_latent();
   const int nview = data()->nview(m_mode);

   log_alpha = alpha.log();
   log_r = - r.log() + (ArrayXXd::Ones(K, nview) - r).log();
}

void SpikeAndSlabPrior::save(std::shared_ptr<const StepFile> sf) const
{
  NormalOnePrior::save(sf);

  std::vector<std::string> paths = sf->getSpikeAndSlabFileNames(m_mode);

  smurff::matrix_io::eigen::write_matrix(paths[0], this->alpha.matrix());
  smurff::matrix_io::eigen::write_matrix(paths[1], this->r.matrix());
}

void SpikeAndSlabPrior::restore(std::shared_ptr<const StepFile> sf)
{
  NormalOnePrior::restore(sf);

  std::vector<std::string> paths = sf->getSpikeAndSlabFileNames(m_mode);

  for (auto path : paths) THROWERROR_FILE_NOT_EXIST(path);

  MatrixXd alpha_as_matrix; // read_matrix cannot read in to Eigen::Array
  smurff::matrix_io::eigen::read_matrix(paths[0], alpha_as_matrix);
  this->alpha = alpha_as_matrix;

  MatrixXd r_as_matrix; // read_matrix cannot read in to Eigen::Array
  smurff::matrix_io::eigen::read_matrix(paths[1], r_as_matrix);
  this->r = r_as_matrix;

  //compute Zkeep
  int d = 0;
  Zkeep.setZero();
  for(int v=0; v<data()->nview(m_mode); ++v) 
  {
      for(int i=0; i<data()->view_size(m_mode, v); ++i, ++d)
      {
          Zkeep.col(v) += (U().col(d).array() > 0).cast<double>(); 
      }
  }
  THROWERROR_ASSERT(d == num_cols());

  initLogRLogAlpha();
}

std::pair<double, double> SpikeAndSlabPrior::sample_latent(int d, int k, const MatrixXd& XX, const VectorXd& yX)
{
    const int v = data()->view(m_mode, d);
    double mu, lambda;

    MatrixXd aXX = alpha.matrix().col(v).asDiagonal();
    aXX += XX;
    std::tie(mu, lambda) = NormalOnePrior::sample_latent(d, k, aXX, yX);

    auto Ucol = U().col(d);
    double z1 = log_r(k,v) -  0.5 * (lambda * mu * mu - std::log(lambda) + log_alpha(k,v));
    double z = 1 / (1 + exp(z1));
    double p = rand_unif(0,1);
    if (Zkeep(k,v) > 0 && p < z) {
        Zcol.local()(k,v)++;
        W2col.local()(k,v) += Ucol(k) * Ucol(k);
    } else {
        Ucol(k) = .0;
    }

    return std::make_pair(mu, lambda);
}

std::ostream &SpikeAndSlabPrior::status(std::ostream &os, std::string indent) const
{
   const int V = data()->nview(m_mode);
   for(int v=0; v<V; ++v) 
   {
       int Zcount = (Zkeep.col(v).array() > 0).count();
       os << indent << m_name << ": Z[" << v << "] = " << Zcount << "/" << num_latent() << std::endl;
   }
   return os;
}
