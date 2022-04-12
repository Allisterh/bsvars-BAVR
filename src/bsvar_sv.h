
#ifndef _BSVAR_SV_H_
#define _BSVAR_SV_H_

#include <RcppArmadillo.h>

Rcpp::List bsvar_sv (
    const int&                    S,          // No. of posterior draws
    const arma::mat&              Y,          // NxT dependent variables
    const arma::mat&              X,          // KxT explanatory variables
    const Rcpp::List&             prior,      // a list of priors - original dimensions
    const arma::field<arma::mat>& VB,        // restrictions on B0
    const Rcpp::List&             starting_values, 
    const bool                    sample_s_ = true
);


Rcpp::List logSDDR_homoskedasticity (
    const Rcpp::List&       posterior,  // a list of posteriors
    const Rcpp::List&       prior,      // a list of priors - original dimensions
    const arma::mat&        Y,          // NxT dependent variables
    const arma::mat&        X,          // KxT explanatory variables
    const bool              sample_s_ = true
);

#endif  // _BSVAR_SV_H_