#ifndef LATENTPRIOR_H
#define LATENTPRIOR_H

#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <memory>

#include "mvnormal.h"
#include "linop.h"
#include "model.h"

class INoiseModel;
class Macau;

 // forward declarationsc
typedef Eigen::SparseMatrix<double> SparseMatrixD;

/** interface */
class ILatentPrior {
  public:
      // c-tor
      ILatentPrior(Factors &m, int p, INoiseModel &n)
          : base_model(m), pos(p), U(m.U(pos)), V(m.U((pos+1)%2)),
            noise(n) {} 
      virtual ~ILatentPrior() {}

      // utility
      int num_latent() const { return base_model.num_latent; }
      //Eigen::MatrixXd::ConstColXpr col(int i) const { return U.col(i); }
      virtual void savePriorInfo(std::string prefix) = 0;

      // work
      virtual void update_prior() = 0;
      virtual double getLinkNorm() { return NAN; };
      virtual double getLinkLambda() { return NAN; };
      virtual bool run_slave() { return false; } // returns true if some work happened...

      void sample_latents();
      virtual void sample_latent(int n) = 0;

  private:
      Factors &base_model;

  protected:
      int pos;
      Eigen::MatrixXd &U, &V;
      INoiseModel &noise;
};

class DenseLatentPrior : public ILatentPrior
{
  public:
     // c-tor
     DenseLatentPrior(DenseMF &m, int p, INoiseModel &n)
         : ILatentPrior(m, p, n), model(m),
           UU(m.UU.at(pos)), VV(m.UU.at((pos+1)%2)),
           UtU(m.UtU.at(pos)), VtV(m.UtU.at((pos+1)%2))
     {
         assert(m.num_fac() == 2);
         Y =  (p==0) ? m.Y : m.Y.transpose();
     }
     virtual ~DenseLatentPrior() {}

     Eigen::MatrixXd Y; // local copy for faster sampling
     DenseMF &model;

     Eigen::MatrixXd CovF, CovL, CovU;
     Eigen::MatrixXd &UU, &VV;
     Eigen::MatrixXd &UtU, &VtV;

};

class SparseLatentPrior : public ILatentPrior
{
  public:
     // c-tor
     SparseLatentPrior(SparseMF &mf, int p, INoiseModel &n)
         : ILatentPrior(mf, p, n), model(mf)
     {
         assert(mf.num_fac() == 2);
         Y = (p==0) ? mf.Y : mf.Y.transpose();
     }
     virtual ~SparseLatentPrior() {}

     SparseMatrixD Y; // local copy for faster sampling

  protected:
     SparseMF &model;
};

/** Prior without side information (pure BPMF) */

class NormalPrior {
  public: 
    Eigen::VectorXd mu; 
    Eigen::MatrixXd Lambda;
    Eigen::MatrixXd WI;
    Eigen::VectorXd mu0;

    int b0;
    int df;

    Eigen::MatrixXd &nU;

  public:
    NormalPrior(Eigen::MatrixXd &U, int num_latent); 
    virtual ~NormalPrior() {}

    virtual void update_prior();
    virtual void savePriorInfo(std::string prefix);

    virtual const Eigen::VectorXd getMu(int) const { return mu; }
    virtual const Eigen::MatrixXd getLambda(int) const { return Lambda; }
};

class SparseNormalPrior : public NormalPrior, public SparseLatentPrior {
  public:
    SparseNormalPrior(SparseMF &m, int p, INoiseModel &n) : NormalPrior(m.U(p), m.num_latent), SparseLatentPrior(m, p, n) {}
    virtual ~SparseNormalPrior() {}

    void sample_latent(int n) override;
    void update_prior() override { NormalPrior::update_prior(); }
    void savePriorInfo(std::string prefix) override { NormalPrior::savePriorInfo(prefix); }
};

class DenseNormalPrior : public NormalPrior, public DenseLatentPrior {
  public:
    DenseNormalPrior(DenseMF &m, int p, INoiseModel &n) : NormalPrior(m.U(p), m.num_latent), DenseLatentPrior(m, p, n) {} 
    virtual ~DenseNormalPrior() {}

    void sample_latent(int n) override;
    void update_prior() override;
    void savePriorInfo(std::string prefix) override { NormalPrior::savePriorInfo(prefix); }
};

/** Prior with side information */
template<class FType>
class MacauPrior : public SparseNormalPrior {
  public:
    Eigen::MatrixXd Uhat;
    std::unique_ptr<FType> F;  /* side information */
    Eigen::MatrixXd FtF;       /* F'F */
    Eigen::MatrixXd beta;      /* link matrix */
    bool use_FtF;
    double lambda_beta;
    double lambda_beta_mu0; /* Hyper-prior for lambda_beta */
    double lambda_beta_nu0; /* Hyper-prior for lambda_beta */

    double tol = 1e-6;

  public:
    MacauPrior(SparseMF &, int, INoiseModel &);
    virtual ~MacauPrior() {}
            
    void addSideInfo(std::unique_ptr<FType> &Fmat, bool comp_FtF = false);

    void update_prior() override;
    double getLinkNorm() override;
    double getLinkLambda() override { return lambda_beta; };
    const Eigen::VectorXd getMu(int n) const override { return this->mu + Uhat.col(n); }
    const Eigen::MatrixXd getLambda(int) const override { return this->Lambda; }

    void compute_Ft_y_omp(Eigen::MatrixXd &);
    virtual void sample_beta();
    void setLambdaBeta(double lb) { lambda_beta = lb; };
    void setTol(double t) { tol = t; };
    void savePriorInfo(std::string prefix) override;
};


/** Prior with side information */
template<class FType>
class MacauMPIPrior : public MacauPrior<FType> {
  public:
    MacauMPIPrior(SparseMF &, int, INoiseModel &);
    virtual ~MacauMPIPrior() {}

    void addSideInfo(std::unique_ptr<FType> &Fmat, bool comp_FtF = false);

    virtual void sample_beta();
    virtual bool run_slave() { sample_beta(); return true; }

    int world_rank;
    int world_size;

    int rhs() const { return rhs_for_rank[world_rank]; }

  private:
    int* rhs_for_rank = NULL;
    double* rec     = NULL;
    int* sendcounts = NULL;
    int* displs     = NULL;
};


typedef Eigen::VectorXd VectorNd;
typedef Eigen::MatrixXd MatrixNNd;
typedef Eigen::ArrayXd ArrayNd;

/** Spike and slab prior */
class SpikeAndSlabPrior : public SparseLatentPrior {
  public:
    VectorNd Zcol, W2col, Zkeep;
    ArrayNd alpha;
    VectorNd r;

    //-- hyper params
    const double prior_beta = 1; //for r
    const double prior_alpha_0 = 1.; //for alpha
    const double prior_beta_0 = 1.; //for alpha
 
  public:
    SpikeAndSlabPrior(SparseMF &, int, INoiseModel &);
    virtual ~SpikeAndSlabPrior() {}

    void update_prior() override;
    void savePriorInfo(std::string prefix) override;
    void sample_latent(int n) override;
};

std::pair<double,double> posterior_lambda_beta(Eigen::MatrixXd & beta, Eigen::MatrixXd & Lambda_u, double nu, double mu);
double sample_lambda_beta(Eigen::MatrixXd & beta, Eigen::MatrixXd & Lambda_u, double nu, double mu);

Eigen::MatrixXd A_mul_B(Eigen::MatrixXd & A, Eigen::MatrixXd & B);
Eigen::MatrixXd A_mul_B(Eigen::MatrixXd & A, SparseFeat & B);
Eigen::MatrixXd A_mul_B(Eigen::MatrixXd & A, SparseDoubleFeat & B);

#endif /* LATENTPRIOR_H */
