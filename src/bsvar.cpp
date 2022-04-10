
#include <RcppArmadillo.h>
#include "progress.hpp"
#include "Rcpp/Rmath.h"

#include "utils.h"
#include "sample_ABhyper.h"

using namespace Rcpp;
using namespace arma;


//' @title Bayesian estimation of a homoskedastic Structural Vector Autoregression via Gibbs sampler
//'
//' @description Estimates the homoskedastic SVAR using the Gibbs sampler proposed by Waggoner & Zha (2003)
//' for the structural matrix \code{B} and the equation-by-equation sampler by Chan, Koop, & Yu (2021)
//' for the autoregressive slope parameters \code{A}. Additionally, the parameter matrices \code{A} and \code{B}
//' follow a Minnesota prior and generalised-normal prior distributions respectively with the matrix-specific
//' overall shrinkage parameters estimated thanks to a 3-level hierarchical prior distribution. 
//' See section \bold{Details} for the model equations.
//' 
//' @details 
//' The homoskedastic SVAR model is given by the reduced form equation:
//' \Sexpr[results=rd, stage=build]{katex::math_to_rd("Y = AX + E")}
//' where \code{Y} is an \code{NxT} matrix of dependent variables, \code{X} is a \code{KxT} matrix of explanatory variables, 
//' \code{E} is an \code{NxT} matrix of reduced form error terms, and \code{A} is an \code{NxK} matrix of autoregressive slope coefficients and parameters on deterministic terms in \code{X}.
//' 
//' The structural equation is given by
//' \Sexpr[results=rd, stage=build]{katex::math_to_rd("BE=U")}
//' where \code{U} is an \code{NxT} matrix of structural form error terms, and
//' \code{B} is an \code{NxN} matrix of contemporaneous relationships between structural shocks in the columns of matrix \code{U}.
//' 
//' Finally, the structural shocks, \code{U}, are temporally and contemporaneously independent and jointly normally distributed with zero mean and unit variances.
//' 
//' @param S a positive integer, the number of posterior draws to be generated
//' @param Y an \code{NxT} matrix, the matrix containing \code{T} observations on \code{N} dependent time series variables
//' @param X a \code{KxT} matrix, the matrix containing \code{T} observations on \code{K = N*p+d} regressors including \code{p} lags of dependent variables and \code{d} deterministic terms
//' @param VB a list of \code{N} matrices determining the unrestricted elements of matrix \code{B}
//' @param prior a list containing the following elements
//' \describe{
//'  \item{A}{an \code{NxK} matrix, the mean of the normal prior distribution for the parameter matrix \code{A}}
//'  \item{A_V_inv}{a \code{KxK} precision matrix of the normal prior distribution for each of the row of the parameter matrix \code{A}. This precision matrix is equation invariant.}
//'  \item{B_V_inv}{an \code{NxN} precision matrix of the generalised-normal prior distribution for the structural matrix \code{B}. This precision matrix is equation invariant.}
//'  \item{B_nu}{a positive integer greater of equal than \code{N}, a shape parameter of the generalised-normal prior distribution for the structural matrix \code{B}}
//'  \item{hyper_nu}{a positive scalar, the shape parameter of the inverted-gamma 2 prior distribution for the two overall shrinkage parameters for matrices \code{B} and \code{A}}
//'  \item{hyper_a}{a positive scalar, the shape parameter of the gamma prior for the two overall shrinkage parameters}
//'  \item{hyper_V}{a positive scalar,  the shape parameter of the inverted-gamma 2 for the level 3 hierarchy of shrinkage parameters}
//'  \item{hyper_S}{a positive scalar,  the scale parameter of the inverted-gamma 2 for the level 3 hierarchy of shrinkage parameters}
//' }
//' @param starting_values a list containing the following elements:
//' \describe{
//'  \item{A}{an \code{NxK} matrix of starting values for the parameter \code{A}}
//'  \item{B}{an \code{NxN} matrix of starting values for the parameter \code{B}}
//'  \item{hyper}{a \code{5}-vector of starting values for the shrinkage hyper-parameters of the hierarchical prior distribution}
//' }
//' 
//' @return A list containing two elements:
//' 
//'  \code{posterior} a list with a collection of \code{S} draws from the posterior distribution generated via Gibbs sampler containing:
//'  \describe{
//'  \item{A}{an \code{NxKxS} array with the posterior draws for matrix \code{A}}
//'  \item{B}{an \code{NxNxS} array with the posterior draws for matrix \code{B}}
//'  \item{hyper}{a \code{5xS} matrix with the posterior draws for the hyper-parameters of the hierarchical prior distribution}
//' }
//' 
//' \code{last_draw} a list with the last draw of the simulation (to be provided as \code{starting_values} to the follow-up run of \code{bsvar}) containing the following objects:
//' \describe{
//'  \item{A}{an \code{NxK} matrix with the last MCMC draw of the parameter matrix \code{A}}
//'  \item{B}{an \code{NxN} matrix with the last MCMC draw of the parameter matrix \code{B}}
//'  \item{hyper}{a \code{5}-vector with the last MCMC draw of the hyper-parameter of the hierarchical prior distribution}
//'  }
//'
//' @seealso [normalisation_wz2003()]
//'
//' @author Tomasz Woźniak \email{wozniak.tom@pm.me}
//' 
//' @references Sampling from the generalised-normal full conditional posterior distribution of matrix \code{B} is implemented using the Gibbs sampler by:
//' 
//' Waggoner, D.F., and Zha, T., (2003) A Gibbs sampler for structural vector autoregressions. \emph{Journal of Economic Dynamics and Control}, \bold{28}, 349--366, \doi{https://doi.org/10.1016/S0165-1889(02)00168-9}.
//'
//' Sampling from the multivariate normal full conditional posterior distribution of each of the \code{A} matrix row is implemented using the sampler by:
//' 
//' Chan, J.C.C., Koop, G, and Yu, X. (2021) Large Order-Invariant Bayesian VARs with Stochastic Volatility.
//' 
//' @export
// [[Rcpp::export]]
Rcpp::List bsvar(
  const int&  S,                        // number of draws from the posterior
  const arma::mat&  Y,                  // NxT dependent variables
  const arma::mat&  X,                  // KxT dependent variables
  const arma::field<arma::mat>& VB,     // N-list
  const Rcpp::List& prior,              // a list of priors
  const Rcpp::List& starting_values     // a list of starting values
) {

  // Progress bar setup
  vec prog_rep_points = arma::round(arma::linspace(0, S, 50));
  Rcout << "**************************************************|" << endl;
  Rcout << " Gibbs sampler for the SVAR model                 |" << endl;
  Rcout << "**************************************************|" << endl;
  Rcout << " Progress of the MCMC simulation for " << S << " draws" << endl;
  Rcout << " Press Esc to interrupt the computations" << endl;
  Rcout << "**************************************************|" << endl;
  Progress p(50, true);
  
  const int N       = Y.n_rows;
  const int K       = X.n_rows;
  
  mat   aux_B       = as<mat>(starting_values["B"]);
  mat   aux_A       = as<mat>(starting_values["A"]);
  vec   aux_hyper   = as<vec>(starting_values["hyper"]);
  
  cube  posterior_B(N, N, S);
  cube  posterior_A(N, K, S);
  mat   posterior_hyper(5, S);
  
  for (int s=0; s<S; s++) {
  
    // Increment progress bar
    if (any(prog_rep_points == s)) p.increment();
    // Check for user interrupts
    if (s % 200 == 0) checkUserInterrupt();
    
    sample_hyperparameters(aux_hyper, aux_B, aux_A, VB, prior);
    sample_A_homosk1(aux_A, aux_B, aux_hyper, Y, X, prior);
    sample_B_homosk1(aux_B, aux_A, aux_hyper, Y, X, prior, VB);
    
    posterior_B.slice(s)    = aux_B;
    posterior_A.slice(s)    = aux_A;
    posterior_hyper.col(s)  = aux_hyper;
  } // END s loop
  
  return List::create(
    _["last_draw"]  = List::create(
      _["B"]        = aux_B,
      _["A"]        = aux_A,
      _["hyper"]    = aux_hyper
    ),
    _["posterior"]  = List::create(
      _["B"]        = posterior_B,
      _["A"]        = posterior_A,
      _["hyper"]    = posterior_hyper
    )
  );
} // END bsvar
