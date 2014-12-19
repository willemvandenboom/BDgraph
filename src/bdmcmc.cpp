#include <R.h>
#include <Rmath.h>
#include <R_ext/Lapack.h>
#include <R_ext/BLAS.h>
#include <R_ext/Utils.h>
#include <sstream>
// #include <iostream>   // std::cout
# include <string>       // std::string, std::to_string
#include <vector>        // for using vector
using namespace std;

extern "C" {

// copying matrix A to matrix B, for arrary with one dimention
void copyMatrix ( double A[], double B[], int *p )
{
	for ( int i = 0; i < *p * *p; i++ ) B[i] = A[i]; 	
}
	
// Takes square matrix A (p x p) and retrieves square submatrix B (p_sub x p_sub), dictated by vector sub
void subMatrix ( double A[], double B[], int sub[], int *p_sub, int *p  )
{
	int i, j;
	for ( i = 0; i < *p_sub; i++ )
		for ( j = 0; j < *p_sub; j++ )
			B[j * *p_sub + i] = A[sub[j] * *p + sub[i]]; 
}

// Takes square matrix A (p x p) and retrieves vector B which is 'sub' th row of matrix A
void subRow ( double A[], double B[], int *sub, int *p )
{
	for ( int j = 0; j < *p; j++ ) B[j] = A[j * *p + *sub]; 	
}

// Takes square matrix A (p x p) and retrieves submatrix B(p_sub x p) which is sub rows of matrix A
void subRows ( double A[], double B[], int sub[], int *p_sub, int *p )
{
	int i, j;
	for ( i = 0; i < *p_sub; i++ )
		for ( j = 0; j < *p; j++ )
			B[j * *p_sub + i] = A[j * *p + sub[i]]; 	
}

// Takes square matrix A (p x p) and retrieves submatrix B(p x p_sub) which is 'sub' columns of matrix A
void subCol ( double A[], double B[], int *sub, int *p )
{
	int i;
	for ( i = 0; i < *p; i++ ) B[i] = A[*sub * *p + i]; 	
}

// Takes square matrix A (p x p) and retrieves submatrix B(p x p_sub) which is 'sub' columns of matrix A
void subCols ( double A[], double B[], int sub[], int *p_sub, int *p )
{
	int i, j;
	for ( i = 0; i < *p; i++ )
		for ( j = 0; j < *p_sub; j++ )
			B[j * *p + i] = A[sub[j] * *p + i]; 	
}

//  Multiplies (p_i x p_k) matrix by (p_k x p_j) matrix to give (p_i x p_j) matrix
//  C := A * B
void multiplyMatrix( double A[], double B[], double C[], int *p_i, int *p_j, int *p_k )
{
	double alpha = 1.0;
	double beta  = 0.0;
	char trans   = 'N';
	// LAPACK function to compute  C := alpha * A * B + beta * C																				
	F77_NAME(dgemm)( &trans, &trans, p_i, p_j, p_k, &alpha, A, p_i, B, p_k, &beta, C, p_i );
}

// creating identity matrix (p x p)
void identityMatrix( double A[], int *p )
{
	int i, j;
	for ( i = 0; i < *p; i++ )
		for ( j = 0; j < *p; j++ )
			A[j * *p + i] = ( i != j ) ? 0.0 : 1.0;
}

// inverse function for symmetric positive-definite matrices (p x p)
// ******** WARNING: this function change matrix A **************************
void inverse( double A[], double A_inv[], int *p )
{
	int info;
	char uplo = 'U';

	// creating an identity matrix
	for ( int i = 0; i < *p; i++ )
		for ( int j = 0; j < *p; j++ )
			A_inv[j * *p + i] = (i != j) ? 0.0 : 1.0;

	// LAPACK function: computes solution to A x X = B, where A is symmetric positive definite matrix
	F77_NAME(dposv)( &uplo, p, p, A, p, A_inv, p, &info );
}

// calculating the maximum difference between two p by p matrices (A and B)
void maxDiff( double A[], double B[], double * max, int *p )
{
	int i;
	double temp;
	*max = fabs( A[0] - B[0] );

	for ( i = 1; i < *p * *p; i++ )
	{
		temp = fabs( A[i] - B[i] );
		if ( temp > *max ) *max = temp;
	}
}

// sampling from Wishart distribution
// Ti = chol( solve( D ) )
void rwish ( double Ti[], double K[], int *b, int *p )
{
	int i, j;
	vector<double> psi( *p * *p ); //double psi[*p * *p];

	// ---- Sample values in Psi matrix ---
    GetRNGstate();
	for ( i = 0; i < *p; i++ )
		for ( j = 0; j < *p; j++ )
			psi[j * *p + i] = (i < j) ? rnorm(0, 1) : ( (i > j) ? 0.0 : sqrt( rchisq( *b + *p - i - 1 ) ) );
	PutRNGstate();
	// ------------------------------------

    // C <- psi %*% Ti 
    vector<double> C( *p * *p ); // double C[*p * *p];
	multiplyMatrix( &psi[0], Ti, &C[0], p, p, p );

	// K <- t(C) %*% C 
	double alpha = 1.0;
	double beta  = 0.0;
	char transA   = 'T';
	char transB   = 'N';
	// LAPACK function to compute  C := alpha * A * B + beta * C																				
	F77_NAME(dgemm)( &transA, &transB, p, p, p, &alpha, &C[0], p, &C[0], p, &beta, K, p );
}

// A is adjacency matrix which has zero in its diagonal
// threshold = 1e-8
void rgwish ( int G[], double Ti[], double K[], int *b, int *p )
{
	double threshold = 1e-8;
	
	int j, k, a, l;
	
	rwish( Ti, K, b, p );
	
	vector<double> Sigma( *p * *p ); // double Sigma[*p * *p];
	inverse( K, &Sigma[0], p );
	
	// copying  matrix sigma to matrix W	
	vector<double> W( *p * *p ); // double W[*p * *p];
	copyMatrix( &Sigma[0], &W[0], p ); 
	
	double difference = 1;	
	while ( difference > threshold )
	{
		// copying  matrix W to matrix W_last	
		vector<double> W_last( *p * *p );  // double W_last[*p * *p];
		copyMatrix( &W[0], &W_last[0], p ); 	
		
		//------------- Big Loop. ----------------
		for ( j = 0; j < *p; j++ )
		{
			//------ Count the neighbors --------
			a = 0;
			for ( k = 0; k < *p; k++ )  a += G[k * *p + j];

			//----- Adjust ----------------------
			if ( a > 0 )
			{
				//----- Record Neighbors --------
				vector<int> N_j( a ); // int N_j[a];
				l = 0;
				for ( k = 0; k < *p; k++ )
					if ( G[k * *p + j] )
					{
						N_j[l] = k;
						l++;
					}
				//--------------------------------

				//------- Some Math --------------
				vector<double> W_N_j( a * a ); // double W_N_j[a * a];
				subMatrix ( &W[0], &W_N_j[0], &N_j[0], &a, p );
				
				vector<double> Sigma_N_j( a ); // double Sigma_N_j[a];
				for ( k = 0; k < a; k++ ) Sigma_N_j[k] = Sigma[j * *p + N_j[k]];
				
				vector<double> W_N_j_inv( a * a ); // double W_N_j_inv[a * a];
				inverse( &W_N_j[0], &W_N_j_inv[0], &a );
				// inverseChol( W_N_j, W_N_j_inv, a ); // ?? I should to check it
				
				vector<double> beta_star_hat_j( a ); // double beta_star_hat_j[a];
				int p_j = 1;
				multiplyMatrix( &W_N_j_inv[0], &Sigma_N_j[0], &beta_star_hat_j[0], &a, &p_j, &a );
				
				vector<double> beta_star( *p ); // double beta_star[*p];
				for ( k = 0; k < *p; k++ ) beta_star[k] = 0.0; // ?? I can make mix this line with next line with ?:
				for ( k = 0; k < a; k++ ) beta_star[N_j[k]] = beta_star_hat_j[k];
				
				vector<double> ww( *p ); // double ww[*p];
				p_j = 1;
				multiplyMatrix( &W[0], &beta_star[0], &ww[0], p, &p_j, p );
				
				for ( k = 0; k < *p; k++ )
					if ( k != j )
					{
						W[k * *p + j] = ww[k];
						W[j * *p + k] = ww[k];
					}
			} else {
				for ( k = 0; k < *p; k++ )
					if ( k != j )
					{
						W[k * *p + j] = 0.0;
						W[j * *p + k] = 0.0;
					}			
			} 
		}//loop through j
		//---------- End Big loop --------------------------			

		maxDiff( &W[0], &W_last[0], &difference, p );
	}//main while loop

	inverse( &W[0], K, p );
}

// ******** Below functions are for calculating birth and death rates *********
// Takes square matrix A (p x p) and retrieves vector B which is 'sub' th row of matrix A, minus 'sub' element
// Likes A[j, -j] in R
void subRowMins ( double A[], double B[], int *sub, int *p )
{
	int j;
	int l = 0;
	for ( j = 0; j < *p; j++ ) 
		if ( j != *sub )
		{
			B[l] = A[j * *p + *sub];
			l++;
		}	
}

// Takes square matrix A (p x p) and retrieves submatrix B(2 x p-2) which is sub rows of matrix A, minus two elements
// Likes A[(i,j), -(i,j)] in R
void subRowsMins ( double A[], double B[], int sub[], int *p_sub, int *p )
{
	int i, j;
	int l = 0;
	
	for ( j = 0; j < *p; j++ )
	{
		for ( i = 0; i < *p_sub; i++ )
			if ( (j != sub[0]) & (j != sub[1]) )
			{
				B[l] = A[j * *p + sub[i]]; 
				l++;
			}
	}	
}

// Takes square matrix A (p x p) and retrieves vector B which is 'sub' th column of matrix A, minus 'sub' element
// Likes A[-j, j] in R
void subColMins ( double A[], double B[], int *sub, int *p )
{
	int i;
	int l = 0;
	for ( i = 0; i < *p; i++ ) 
		if ( i != *sub )
		{
			B[l] = A[*sub * *p + i]; 
			l++;
		}		
}

// Takes square matrix A (p x p) and retrieves submatrix B(p-2 x 2) which is 'sub' columns of matrix A, minus two elements
// Likes A[-(i,j), (i,j)] in R
void subColsMins( double A[], double B[], int sub[], int *p_sub, int *p  )
{
	int i, j;
	int l = 0;
	
	for ( j = 0; j < *p_sub; j++ )
	{
		for ( i = 0; i < *p; i++ )
			if ( (i != sub[0]) & (i != sub[1]) )
			{		
				B[l] = A[sub[j] * *p + i]; 
				l++;
			}
	}	
}

// Takes square matrix A (p x p) and retrieves square submatrix B (p_sub x p_sub), dictated by vector sub
// likes A[-j, -j] in R
void subMatrixMins1 ( double A[], double B[], int *sub, int *p )
{
	int i, j;
	int l = 0;
	
	for ( j = 0; j < *p; j++ )
		for ( i = 0; i < *p; i++ )
			if ( (i != *sub) & (j != *sub) )
			{
				B[l] = A[j * *p + i];
				l++; 
			}
}

// Takes square matrix A (p x p) and retrieves square submatrix B (p_sub x p_sub), dictated by vector sub
// Likes A[-(i,j), -(i,j)] in R
void subMatrixMins2 ( double A[], double B[], int sub[], int *p )
{
	int i, j;
	int l = 0;
	
	for ( j = 0; j < *p; j++ )
		for ( i = 0; i < *p; i++ )
			if ( (i != sub[0]) & (i != sub[1]) & (j != sub[0]) & (j != sub[1]) )
			{
				B[l] = A[j * *p + i];
				l++; 
			}
}

// Likes sum( A * ( B - C ) ) in R
void sumDiagAB( double A[], double B[], double C[], double *sumDiag, int *p )
{
	int i;
	*sumDiag = 0;
	
	for ( i = 0; i < *p * *p ; i++ )
		*sumDiag += A[i] * ( B[i] - C[i] );

//	for ( i = 0; i < *p ; i++ )
//		for ( j = 0; j < *p; j++ )
//			sumDiag += A[j * *p + i] * ( B[j * *p + i] - C[j * *p + i] );		
} 

// Minus (p x p) matrix by (p x p) matrix to give (p x p) matrix
// C := A - B
void minusMatrix( double A[], double B[], double C[], int *p )
{
	for ( int i = 0; i < *p * *p ; i++ ) C[i] = A[i] - B[i];	
}

// e = (i,j) 
// K[e, -e] %*% solve(K[-e, -e]) %*% t( K[e, -e] ) 
void K121output( double K[], double K121[], int e[], int *p )
{
	int two = 2;
	int p2  = *p - 2;
	
	// K12    <- K[e, -e]  
	vector<double> K12( 2 * p2 ); // double K12[2 * p2];
	subRowsMins( K, &K12[0], e, &two, p );
//printMatrix( K, p );
	// K[-e, -e]
	vector<double> K22( p2 * p2 ); // double K22[p2 * p2];
	subMatrixMins2( K, &K22[0], e, p );
	
	// solve( K[-e, -e] )
	vector<double> K22_inv( p2 * p2 ); // double K22_inv[p2 * p2];
	inverse( &K22[0], &K22_inv[0], &p2 );
	
	// K12 %*% K22_inv
	vector<double> K12xK22_inv( 2 * p2 ); // double K12xK22_inv[2 * p2];
	multiplyMatrix( &K12[0], &K22_inv[0], &K12xK22_inv[0], &two, &p2, &p2 );
	
	// K121 <- K12 %*% solve(K[-e, -e]) %*% t(K12) 
	double alpha = 1.0;
	double beta  = 0.0;
	char transA   = 'N';
	char transB   = 'T';
	// F77_NAME(dgemm)( &trans, &trans,p_i,  p_j,  p_k, &alpha, A,           p_i,  B,   p_k, &beta, C,    p_i );																				
	F77_NAME(dgemm)( &transA, &transB, &two, &two, &p2, &alpha, &K12xK22_inv[0], &two, &K12[0], &two, &beta, K121, &two );			
}

// K[j, -j] %*% solve(K[-j, -j]) %*% t( K[j, -j] ) 
void K111output( double K[], double *K111, int *j, int *p )
{
	int one = 1;
	int p1  = *p - 1;
	
	// K12    <- K[j, -j]  
	vector<double> K12( p1 ); // double K12[p1];
	subRowMins( K, &K12[0], j, p );
	
	// K[-j, -j]
	vector<double> K22( p1 * p1 ); // double K22[p1 * p1];
	subMatrixMins1( K, &K22[0], j, p );
	
	// solve( K[-j, -j] )
	vector<double> K22_inv( p1 * p1 ); // double K22_inv[p1 * p1];
	inverse( &K22[0], &K22_inv[0], &p1 );
	
	// K12 %*% K22_inv
	vector<double> K12xK22_inv( p1 ); // double K12xK22_inv[p1];
	multiplyMatrix( &K12[0], &K22_inv[0], &K12xK22_inv[0], &one, &p1, &p1 );
	
	// K121 <- K12 %*% solve(K[-e, -e]) %*% t(K12) 
	double alpha = 1.0;
	double beta  = 0.0;
	char transA   = 'N';
	char transB   = 'T';
	// F77_NAME(dgemm)( &trans, &trans,p_i,  p_j,  p_k, &alpha, A,           p_i,  B,   p_k, &beta, C,    p_i );																				
	F77_NAME(dgemm)( &transA, &transB, &one, &one, &p1, &alpha, &K12xK22_inv[0], &one, &K12[0], &one, &beta, K111, &one );			
}

//////////////////////////////////////////////////////////////////////////////
// computing birth or death rate of element (i,j)
void log_H_ij( double K[], int G[], double *log_Hij, int *i, int *j, int *bstar, double Ds[], int *p )
{
	double Dsii   = Ds[*i * *p + *i];
	double Dsjj   = Ds[*j * *p + *j];
	double Dsij   = Ds[*j * *p + *i];
	vector<double> Dsee( 4 );
	Dsee[0] = Dsii; 
	Dsee[1] = Dsij; 
	Dsee[2] = Dsij; 
	Dsee[3] = Dsjj; 
	

//	int one = 1;
	int two = 2;
//	int p1  = *p - 1;
//	int p2  = *p - 2;
	vector<int> e( 2 );
	e[0] = *i;
	e[1] = *j;

	vector<double> Kcopy( *p * *p ); // double Kcopy[*p * *p];
	copyMatrix( K, &Kcopy[0], p );
 // printMatrix( K, p );	
	if ( G[*j * *p + *i] == 0 )
	{
		// F <- K12 %*% solve(K[-e, -e]) %*% t(K12) 
		vector<double> F( two * two ); // double F[two * two];
		K121output( K, &F[0], &e[0], p );
		//printMatrix( K, p );
		// K[e, e]	
		vector<double> Kee( two * two ); // double Kee[two * two];
		subMatrix( K, &Kee[0], &e[0], &two, p );

		// a <- K[e, e] - F	
		vector<double> a( two * two ); // double a[two * two];	
		minusMatrix( &Kee[0], &F[0],  &a[0], &two );
		
		// sig <- sqrt( a[1, 1] / Dsjj )
		double sig;
		sig = sqrt( a[0] / Dsjj );

		// mu = - ( Dsij * a[1, 1] ) / Dsjj
		double mu;
		mu = - ( Dsij * a[0] ) / Dsjj;
		
		// u <- rnorm(1, mu, sig)
		double u;
		u = rnorm( mu, sig );
		
		// v <- rgamma(1, shape = bstar / 2, scale = 2 / Ds[j,j])
		double v;
		v = rgamma( *bstar / 2, 2 / Dsjj );
		
		// K[i,j] <- u + F[1,2]
		Kcopy[*j * *p + *i] = u + F[2];
		// K[j,i] <- u + F[1,2]
		Kcopy[*i * *p + *j] = u + F[2];		
		// K[j,j] <- v + u ^ 2 / a[1,1] + F[2,2]
		Kcopy[*j * *p + *j] = v + pow( u, 2 ) / a[0] + F[3];	
	}
	
	// # (i,j) = 0
	// K0 <- K
	vector<double> K0( *p * *p ); // double K0[*p * *p];
	copyMatrix( &Kcopy[0], &K0[0], p );

	// K0[i, j] <- 0
	K0[*j * *p + *i] = 0;
	// K0[j, i] <- 0
	K0[*i * *p + *j] = 0;
	
	// K0_ij22  <- K_12 %*% solve( K0[-j, -j] ) %*% t(K_12)
	double K0_ij22;
	K111output( &K0[0], &K0_ij22, j, p );

	// K0_ij = diag( c( K[i, i], K0_ij22 ) ) 
	double K0_ij[] = { Kcopy[*i * *p + *i], 0.0, 0.0, K0_ij22 };

	// # (i,j) = 1
	// K_12  <- K[e, -e]
	// K1_ij <- K_12 %*% solve( K[-e, -e] ) %*% t(K_12) 
	vector<double> K1_ij( two * two ); // double K1_ij[two * two];
	K121output( &Kcopy[0], &K1_ij[0], &e[0], p );

	// a11 <- K[i, i] - K1_ij[1, 1]
	double a11 = Kcopy[*i * *p + *i] - K1_ij[0];
    
// sumDiagAB( double A[], double B[], double C[], double *sumDiag, int *p )
	double sumDiag;
	sumDiagAB( &Dsee[0], K0_ij, &K1_ij[0], &sumDiag, &two );

//   log_Hij = ( log(Dsjj) - log(a11) + ( Dsii - Dsij ^ 2 / Dsjj ) * a11 -
//		        sum( Dsee * ( K0_ij - K1_ij ) ) ) / 2  

	*log_Hij = ( log(Dsjj) - log(a11) + ( Dsii - pow( Dsij, 2 ) / Dsjj ) * a11 - sumDiag ) / 2 ;
}

// Colculating all the birth and death rates
void ratesMatrix( double K[], double K_prop[], int G[], double rates[], int *b, int *bstar, double D[], double Ds[], int *p )
{
	int i, j;
	double logHij, logI_p;
	
//	for ( i = 0; i < p * p; i++ ) rates[i] = 0.0;
	for ( i = 0; i < *p; i++ )
		for ( j = i + 1; j < *p; j++ )
		{
			log_H_ij( K,      G, &logHij, &i, &j, bstar, Ds, p );
			log_H_ij( K_prop, G, &logI_p, &i, &j, b,     D,  p );
			
			// rates[i,j] = if( G[i,j] == 1 ) exp( logHij - logI_p ) else exp( logI_p - logHij )
			if ( G[j * *p + i] == 1 )
			{
				rates[j * *p + i] = exp( logHij - logI_p );
			} else {
				rates[j * *p + i] = exp( logI_p - logHij );
			}
		}
}

//////////////////////////////////////////////////////////////////////////////
// transfer upper elements of matrix G to string object likes: "0111001"
void adjToString( int G[], string * stringG, int *p )
{
	vector<char> charG( *p * ( *p - 1 ) / 2 ); // char stringG[pp];
	
	int i, j;
	int l = 0;

	for ( j = 1; j < *p; j++ )
		for ( i = 0; i < j; i++ )
		{	
			charG[l] = G[j * *p + i] + '0';
			l++;
		}
	
	*stringG = std::string( charG.begin(), charG.end() );	
}   
   
// sum of upper elements of matrix A
void sumUpperMatrix( double A[], double *sumUpper, int *p )
{
	for ( int i = 0; i < *p - 1; i++ )
		for ( int j = i + 1; j < *p; j++ )
			*sumUpper += A[j * *p + i];
}

// select the edge with the maximum rate
void selectEdge( double rates[], int selectedEdge[], int *p )
{
	int i, j;
	double maxRates = -1.7e307; // rates[1,2] in R

	for ( i = 0; i < *p - 1; i++ )
		for ( j = i + 1; j < *p; j++ )
			if ( rates[j * *p + i] > maxRates )
			{
				maxRates = rates[j * *p + i]; 
				selectedEdge[0] = i;
				selectedEdge[1] = j;
			}
}
// Sum (p x p) matrix by (p x p) matrix to give (p x p) matrix
// A := A + B
void sumMatrix( double A[], double B[], int *p )
{
	for ( int i = 0; i < *p * *p ; i++ ) A[i] += B[i];	
}

// 
void whichOne( string sampleG[], string *indG, int *thisOne, int *sizeSampleG )
{
	for ( int i = 0; i < *sizeSampleG; i++ )
		if ( sampleG[i] == *indG )
			*thisOne = i; 
}

///////////////////////////////////////////////////////////////////////////////
// main BD-MCMC algorithm for GGM
////// Input ///////////////////////////////
// iter : number of mcmc iteration
// burnin : number of burn-in
// G : adjency matrix for initial graph
// T : symetric matrix which T = chol( solve( D ) )
// K : initial percision matrix
// p: number of variables
// b and D: parameters of prior distribution of G-Wishart, K ~ W_G( b, D )
// bstar and Ds: parameters of posterior distribution of G-Wishart, K ~ W_G( b, D )
// threshold : threshold for sampling from G-Wishart distribution based on exact sampling method
///////// output ////////////////////////////
// allGraphs : all adjency matrices which is saved as strings likes "011011"
// sampleGraphs : adjency matrices of graphs which are visited by algorithm. 
// allWeights: all weights of graphs.
// graphWeights : weights for the graphs which are visited by algorithm.
// sizeSampleG: number of different graphs that algorithm visited them.
/////////////////////////////////////////////
void bdmcmcDmh( int *iter, int *burnin, int G[], double Ti[], double Ts[], double K[], int *p, 
			 string allGraphs[], double allWeights[], double Ksum[], 
			 string sampleGraphs[], double graphWeights[], int *sizeSampleG,
			 int lastGraph[], double lastK[],
			 int *b, int *bstar, double D[], double Ds[] )
{
	int g;
	//string indG;
	int thisOne;

	vector<double> K_prop( *p * *p ); // double K_prop[*p * *p];
	int selectedEdge[2];
	
	vector<double> rates( *p * *p, 0.0 ); // double rates[*p * *p];
//	for ( int i = 0; i < *p * *p; i++ )		rates[i] = 0.0;	
	
	for ( g = 0; g < *iter; g++ )
	{
		if ( ( g + 1 ) % 1000 == 0 ) Rprintf( "  Iteration  %d          \n", g + 1 );
		//	cout << "   Iteration " << g + 1 << "\n";

		// using exchange algorithm
		// K_prop <- rgwish.exact( G = G + t(G), b = b, T = Ti, p = p )
		// rgwish ( int G[], double Ti[], double K[], int *b, int *p )
		rgwish( G, Ti, &K_prop[0], b, p );
		
		// computing birth and death rates
		// ratesMatrix( double K[], double K_prop[], double G[], double rates[], int *b, int *bstar, double D[], double Ds[], int *p )
		ratesMatrix( K, &K_prop[0], G, &rates[0], b, bstar, D, Ds, p );

		// indG        <- paste( G[upper.tri(G)], collapse = '' )
		// double G[], string *stringG, int *p
		adjToString( G, &allGraphs[g], p );
		
		//all.G       <- c( all.G, indG )
		//allGraphs[g] = indG;
		// allWeights <- c( allWeights, 1 / sum(rates) )
		// double A[], double *sumUpper, int *p
		sumUpperMatrix( &rates[0], &allWeights[g], p );
		allWeights[g] = 1 / allWeights[g];
		
		if ( g > *burnin )
		{
			//Ksum <- Ksum + K
			// A := A + B
			// double A[], double B[], int *p 
			sumMatrix( Ksum, K, p );
			
			// wh   <- which( sample.G == indG )
			// whichOne( string sampleGraphs[], string *indG, int *thisOne, int *sizeSampleG )
			thisOne = *iter;
			whichOne( sampleGraphs, &allGraphs[g], &thisOne, sizeSampleG );
			
			if ( ( thisOne == *iter ) or ( sizeSampleG == 0 ) )
			{
				sampleGraphs[*sizeSampleG] = allGraphs[g];
				graphWeights[*sizeSampleG] = allWeights[g];
				(*sizeSampleG)++;				
			} else {
				// graphWeights[wh] <- graphWeights[wh] + 1 / sum(rates)
				graphWeights[thisOne] += allWeights[g];
			}
		}

		// To select new graph
		//edge    <- which( rates == max(rates) )[1]
		selectEdge( &rates[0], selectedEdge, p );

		// G[edge] <- 1 - G[edge]
		G[selectedEdge[1] * *p + selectedEdge[0]] = 1 - G[selectedEdge[1] * *p + selectedEdge[0]];
		G[selectedEdge[0] * *p + selectedEdge[1]] = 1 - G[selectedEdge[0] * *p + selectedEdge[1]];

		// K <- rgwish.exact(G = G + t(G), b = bstar, T = Ts, p = p)
		rgwish( G, Ts, K, bstar, p );
	}
	// For last graph and its precision matrix
	//copyMatrix( G, lastGraph, p );
	for ( int g = 0; g < *p * *p; g++ ) lastGraph[g] = G[g];	
	copyMatrix( K, lastK, p );	
}
   
////////////////////////////////////////////////////////////////////////////////
// Copula Gaussian graphical models
////////////////////////////////////////////////////////////////////////////////
// for copula function
void getMean( double Z[], double K[], double *muij, double *sigma, int *i, int *j, int *n, int *p )
{
	*muij = 0;
	
	for ( int k = 0; k < *p; k++ ) 
		if ( k != *j )
			*muij += Z[k * *n + *i] * K[*j * *p + k];
			// muij = muij + Z[i,k] * K[k,j]

	*muij = - *muij * *sigma;
}

// for copula function
void getBounds( double Z[], int R[], double *lb, double *ub, int *i, int *j, int *n )
{
	*lb = -1e308;
	*ub = +1e308;

	for ( int k = 0; k < *n; k++ )
	{
     // if ( R[k, j] < R[i, j] ) lb = max( Z[ k, j], lb )
		if ( R[*j * *n + k] < R[*j * *n + *i] )
			*lb = max( Z[*j * *n + k], *lb ) ;
			
	//  if ( R[k, j] > R[i, j] ) ub = min( Z[ k, j], ub )										
		if ( R[*j * *n + k] > R[*j * *n + *i] ) 
			*ub = min( Z[*j * *n + k], *ub );
	}
}
 
// copula part
void copula( double Z[], double K[], int R[], int *n, int *p )
{
	int i, j;
	
	double sigma, sdj, muij;
	
	double lb;
	double ub;
	double runifValue;
	double pnormlb;
	double pnormub;
	
	for ( j = 0; j < *p; j++ )
	{   
		sigma = 1 / K[j * *p + j];
		sdj   = sqrt( sigma );
		
		// interval and sampling
		for( i = 0; i < *n; i++ )
		{
			// getMean( double Z[], double K[], double *muij, double *sigma, int *i, int *j, int *p )
			getMean( Z, K, &muij, &sigma, &i, &j, n, p );
			
			// getBounds( double Z[], int R[], double *lb, double *ub, int *i, int *j, int *n )
			getBounds( Z, R, &lb, &ub, &i, &j, n );
			
			// runifValue = runif( 1, pnorm( lowerBound, muij, sdj ), pnorm( upperBound, muij, sdj ) )
			// Z[i,j]     = qnorm( runifValue, muij, sdj )									
			GetRNGstate();
			pnormlb       = pnorm( lb, muij, sdj, TRUE, FALSE );
			pnormub       = pnorm( ub, muij, sdj, TRUE, FALSE );
			runifValue    = runif( pnormlb, pnormub );
			Z[j * *n + i] = qnorm( runifValue, muij, sdj, TRUE, FALSE );
			PutRNGstate();				
		}
	}
}

// for bdmcmcCopulaDmh function
void getDs( double K[], double Z[], int R[], double D[], double Ds[], int *n, int *p )
{
	// |------- First step: copula 
	// here we will use a copula function
	// Z <- copula( Z = Z, K = K, R = R, n = n, p = p )
	// copula( double Z[], double K[], int R[], int *n, int *p )
	copula( Z, K, R, n, p );
	
	vector<double> S( *p * *p ); // double S[*p * *p];
	// S <- t(Z) %*% Z
	// Here, I'm using Ds instead of S, to saving memory
	double alpha = 1.0;
	double beta  = 0.0;
	char transA   = 'T';
	char transB   = 'N';
	// LAPACK function to compute  C := alpha * A * B + beta * C
	//        DGEMM ( TRANSA,  TRANSB, M, N, K,  ALPHA, A,LDA,B, LDB,BETA, C, LDC )																				
	F77_NAME(dgemm)( &transA, &transB, p, p, n, &alpha, Z, n, Z, n, &beta, &S[0], p );		
	// Ds = D + S
	// Or Ds = D + Ds
	// A := A + B
	//sumMatrix( Ds, D, p );	
	for ( int i = 0; i < *p; i++ )
		for ( int j = 0; j < *p; j++ )
			Ds[ j * *p + i ] = D[ j * *p + i ] + S[ j * *p + i ];
}

// Cholesky decomposition of symmetric positive-definite matrix
// Note: the matrix you pass this function is overwritten with the result.
// A = U' %*% U
void cholesky( double A[], double U[], int *p )
{
	char uplo = 'U';
	int i, j;
	int info;
	// copying  matrix A to matrix L	
	for ( i = 0; i < *p * *p; i++ ) 
		U[i] = A[i]; 	
	
	//----Use Lapack-----
	F77_NAME(dpotrf)( &uplo, p, U, p, &info );	
	//-------------------

	//---- Above lapack function doesn't zero out the lower diagonal -----
	for ( i = 0; i < *p; i++ )
		for ( j = 0; j < i; j++ )
			U[j * *p + i] = 0.0;
}

// for bdmcmcCopulaDmh function
void getTs( double Ds[], double Ts[], int *p )
{
	vector<double> invDs( *p * *p ); // double invDs[*p * *p];
	vector<double> copyDs( *p * *p ); // double copyDs[*p * *p];

	// invDs = solve(Ds)	
	copyMatrix( Ds, &copyDs[0], p );
	inverse( &copyDs[0], &invDs[0], p );	

	// Ts = chol(invDs)
	cholesky( &invDs[0], Ts, p );	
}

///////////////////////////////////////////////////////////////////////////////
void bdmcmcCopulaDmh( int *iter, int *burnin, int G[], double Ti[], double Ts[], double K[], int *p, 
			 double Z[], int R[], int *n,
			 string allGraphs[], double allWeights[], double Ksum[], 
			 string sampleGraphs[], double graphWeights[], int *sizeSampleG,
			 int lastGraph[], double lastK[],
			 int *b, int *bstar, double D[], double Ds[] )
{
	int g;
	int thisOne;

	vector<double> K_prop( *p * *p ); // double K_prop[*p * *p];
	int selectedEdge[2];
	
	vector<double> rates( *p * *p, 0.0 ); // double rates[*p * *p];
//	for ( int i = 0; i < *p * *p; i++ )		rates[i] = 0.0;	
	
	for ( g = 0; g < *iter; g++ )
	{
		if ( ( g + 1 ) % 1000 == 0 ) Rprintf( "  Iteration  %d          \n", g + 1 );
		//	cout << "   Iteration " << g + 1 << "\n";

		// |------- First step: copula 
		// here we will use a copula function
		// getDs( double K[], double Z[], int R[], double D[], double Ds[], int *n, int *p )
		getDs( K, Z, R, D, Ds, n, p );

	    // Ts = chol( solve(Ds) )
		// getTs( double Ds[], double Ts[], int *p )
		getTs( Ds, Ts, p );

		// using exchange algorithm
		// K_prop <- rgwish.exact( G = G + t(G), b = b, T = Ti, p = p )
		// rgwish ( double A[], double Ti[], double K[], int *b, int *p )
		rgwish( G, Ti, &K_prop[0], b, p );
		
		// computing birth and death rates
		// ratesMatrix( double K[], double K_prop[], double G[], double rates[], int *b, int *bstar, double D[], double Ds[], int *p )
		ratesMatrix( K, &K_prop[0], G, &rates[0], b, bstar, D, Ds, p );

		// indG        <- paste( G[upper.tri(G)], collapse = '' )
		// double G[], string *stringG, int *p
		adjToString( G, &allGraphs[g], p );
		
		//all.G       <- c( all.G, indG )
		//allGraphs[g] = indG;
		// allWeights <- c( allWeights, 1 / sum(rates) )
		// double A[], double *sumUpper, int *p
		sumUpperMatrix( &rates[0], &allWeights[g], p );
		allWeights[g] = 1 / allWeights[g];
		
		if ( g > *burnin )
		{
			//Ksum <- Ksum + K
			// A := A + B
			// double A[], double B[], int *p 
			sumMatrix( Ksum, K, p );
			
			// wh   <- which( sample.G == indG )
			// whichOne( string sampleGraphs[], string *indG, int *thisOne, int *sizeSampleG )
			thisOne = *iter;
			whichOne( sampleGraphs, &allGraphs[g], &thisOne, sizeSampleG );
			
			if ( ( thisOne == *iter ) or ( sizeSampleG == 0 ) )
			{
				sampleGraphs[*sizeSampleG] = allGraphs[g];
				graphWeights[*sizeSampleG] = allWeights[g];
				(*sizeSampleG)++;				
			} else {
				// graphWeights[wh] <- graphWeights[wh] + 1 / sum(rates)
				graphWeights[thisOne] += allWeights[g];
			}
		}

		// To select new graph
		//edge    <- which( rates == max(rates) )[1]
		selectEdge( &rates[0], selectedEdge, p );

		// G[edge] <- 1 - G[edge]
		G[selectedEdge[1] * *p + selectedEdge[0]] = 1 - G[selectedEdge[1] * *p + selectedEdge[0]];
		G[selectedEdge[0] * *p + selectedEdge[1]] = 1 - G[selectedEdge[0] * *p + selectedEdge[1]];

		// K <- rgwish.exact(G = G + t(G), b = bstar, T = Ts, p = p)
		rgwish( G, Ts, K, bstar, p );
	}
	// For last graph and its precision matrix
	// copyMatrix( G, lastGraph, p );
	for ( int g = 0; g < *p * *p; g++ ) lastGraph[g] = G[g];
	copyMatrix( K, lastK, p );	
}
   
////////////////////////////////////////////////////////////////////////////////
// for copula function with missing data 
void getBoundsNA( double Z[], int R[], double *lb, double *ub, int *i, int *j, int *n )
{
	*lb = -1e308;
	*ub = +1e308;

	for ( int k = 0; k < *n; k++ )
	{
		if ( R[*j * *n + k] != 0 )
		{
			// if ( R[k, j] < R[i, j] ) lb = max( Z[ k, j], lb )
			if ( R[*j * *n + k] < R[*j * *n + *i] )
				*lb = max( Z[*j * *n + k], *lb );
				
			//  if ( R[k, j] > R[i, j] ) ub = min( Z[ k, j], ub )																	
			if ( R[*j * *n + k] > R[*j * *n + *i] ) 
				*ub = min( Z[*j * *n + k], *ub );
		}
	}
}
 
// copula part
void copulaNA( double Z[], double K[], int R[], int *n, int *p )
{
	int i, j;
	
	double sigma, sdj, muij;
	
	double lb;
	double ub;
	double runifValue;
	double pnormlb;
	double pnormub;
	
	for ( j = 0; j < *p; j++ )
	{   
		sigma = 1 / K[j * *p + j];
		sdj   = sqrt( sigma );
		
		// interval and sampling
		for( i = 0; i < *n; i++ )
		{
			// getMean( double Z[], double K[], double *muij, double *sigma, int *i, int *j, int *p )
			getMean( Z, K, &muij, &sigma, &i, &j, n, p );
			
			GetRNGstate();
			// if (  R[i, j] != 0 ) #( !is.na( R[i, j] ) )
			if ( R[j * *n + i] != 0 )
			{
				// getBounds( double Z[], int R[], double *lb, double *ub, int *i, int *j, int *n )
				getBoundsNA( Z, R, &lb, &ub, &i, &j, n );
				
				// runifValue = runif( 1, pnorm( lowerBound, muij, sdj ), pnorm( upperBound, muij, sdj ) )
				// Z[i,j]     = qnorm( runifValue, muij, sdj )									
				pnormlb       = pnorm( lb, muij, sdj, TRUE, FALSE );
				pnormub       = pnorm( ub, muij, sdj, TRUE, FALSE );
				runifValue    = runif( pnormlb, pnormub );
				Z[j * *n + i] = qnorm( runifValue, muij, sdj, TRUE, FALSE );
			} else {
				// Z[i, j] = rnorm( 1, muj[i], sdj )
				Z[j * *n + i] = rnorm( muij, sdj );
			}
			PutRNGstate();				
		}
	}
}

// for bdmcmcCopulaDmh function
void getDsNA( double K[], double Z[], int R[], double D[], double Ds[], int *n, int *p )
{
	// |------- First step: copula 
	// here we will use a copula function
	// Z <- copula( Z = Z, K = K, R = R, n = n, p = p )
	// copula( double Z[], double K[], int R[], int *n, int *p )
	copulaNA( Z, K, R, n, p );
	
	vector<double> S( *p * *p ); // double S[*p * *p];
	// S <- t(Z) %*% Z
	// Here, I'm using Ds instead of S, to saving memory
	double alpha = 1.0;
	double beta  = 0.0;
	char transA   = 'T';
	char transB   = 'N';
	// LAPACK function to compute  C := alpha * A * B + beta * C
	//        DGEMM ( TRANSA,  TRANSB, M, N, K,  ALPHA, A,LDA,B, LDB,BETA, C, LDC )																				
	F77_NAME(dgemm)( &transA, &transB, p, p, n, &alpha, Z, n, Z, n, &beta, &S[0], p );		
	// Ds = D + S
	// Or Ds = D + Ds
	// A := A + B
	//sumMatrix( Ds, D, p );	
	for ( int i = 0; i < *p; i++ )
		for ( int j = 0; j < *p; j++ )
			Ds[ j * *p + i ] = D[ j * *p + i ] + S[ j * *p + i ];
}

// copula approach for data with missing values
///////////////////////////////////////////////////////////////////////////////
void bdmcmcCopulaDmhNA( int *iter, int *burnin, int G[], double Ti[], double Ts[], double K[], int *p, 
			 double Z[], int R[], int *n,
			 string allGraphs[], double allWeights[], double Ksum[], 
			 string sampleGraphs[], double graphWeights[], int *sizeSampleG,
			 int lastGraph[], double lastK[],
			 int *b, int *bstar, double D[], double Ds[] )
{
	int g;
	int thisOne;

	vector<double> K_prop( *p * *p ); // double K_prop[*p * *p];
	int selectedEdge[2];
	
	vector<double> rates( *p * *p, 0.0 ); // double rates[*p * *p];
//	for ( int i = 0; i < *p * *p; i++ )		rates[i] = 0.0;	
	
	for ( g = 0; g < *iter; g++ )
	{
		if ( ( g + 1 ) % 1000 == 0 ) Rprintf( "  Iteration  %d          \n", g + 1 );
		//	cout << "   Iteration " << g + 1 << "\n";

		// |------- First step: copula 
		// here we will use a copula function
		// getDs( double K[], double Z[], int R[], double D[], double Ds[], int *n, int *p )
		getDsNA( K, Z, R, D, Ds, n, p );

	    // Ts = chol( solve(Ds) )
		// getTs( double Ds[], double Ts[], int *p )
		getTs( Ds, Ts, p );

		// using exchange algorithm
		// K_prop <- rgwish.exact( G = G + t(G), b = b, T = Ti, p = p )
		// rgwish ( double A[], double Ti[], double K[], int *b, int *p )
		rgwish( G, Ti, &K_prop[0], b, p );
		
		// computing birth and death rates
		// ratesMatrix( double K[], double K_prop[], double G[], double rates[], int *b, int *bstar, double D[], double Ds[], int *p )
		ratesMatrix( K, &K_prop[0], G, &rates[0], b, bstar, D, Ds, p );

		// indG        <- paste( G[upper.tri(G)], collapse = '' )
		// double G[], string *stringG, int *p
		adjToString( G, &allGraphs[g], p );
		
		//all.G       <- c( all.G, indG )
		//allGraphs[g] = indG;
		// allWeights <- c( allWeights, 1 / sum(rates) )
		// double A[], double *sumUpper, int *p
		sumUpperMatrix( &rates[0], &allWeights[g], p );
		allWeights[g] = 1 / allWeights[g];
		
		if ( g > *burnin )
		{
			//Ksum <- Ksum + K
			// A := A + B
			// double A[], double B[], int *p 
			sumMatrix( Ksum, K, p );
			
			// wh   <- which( sample.G == indG )
			// whichOne( string sampleGraphs[], string *indG, int *thisOne, int *sizeSampleG )
			thisOne = *iter;
			whichOne( sampleGraphs, &allGraphs[g], &thisOne, sizeSampleG );
			
			if ( ( thisOne == *iter ) or ( sizeSampleG == 0 ) )
			{
				sampleGraphs[*sizeSampleG] = allGraphs[g];
				graphWeights[*sizeSampleG] = allWeights[g];
				(*sizeSampleG)++;				
			} else {
				// graphWeights[wh] <- graphWeights[wh] + 1 / sum(rates)
				graphWeights[thisOne] += allWeights[g];
			}
		}

		// To select new graph
		//edge    <- which( rates == max(rates) )[1]
		selectEdge( &rates[0], selectedEdge, p );

		// G[edge] <- 1 - G[edge]
		G[selectedEdge[1] * *p + selectedEdge[0]] = 1 - G[selectedEdge[1] * *p + selectedEdge[0]];
		G[selectedEdge[0] * *p + selectedEdge[1]] = 1 - G[selectedEdge[0] * *p + selectedEdge[1]];

		// K <- rgwish.exact(G = G + t(G), b = bstar, T = Ts, p = p)
		rgwish( G, Ts, K, bstar, p );
	}
	// For last graph and its precision matrix
	// copyMatrix( G, lastGraph, p );
	for ( int g = 0; g < *p * *p; g++ ) lastGraph[g] = G[g];
	copyMatrix( K, lastK, p );	
}
   
////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// Takes square matrix A (p x p) and retrieves value as below
// sum( A[i,] )
void sumRow ( int A[], int *sumRowi, int *i, int *p )
{
	int j;
	*sumRowi = 0;
	for ( j = 0; j < *p; j++ ) *sumRowi += A[j * *p + *i]; 	
}

//////////////////////////////////////////////////////////////////////////////
// For bdmcmcApprox algorithm: computing birth or death rate of element (i,j)
void logHijApprox( double K[], int G[], double *HijApprox, int *i, int *j, int *b, int *bstar, double Ti[], double Ds[], int *p )
{
	double Dsii   = Ds[*i * *p + *i];
	double Dsjj   = Ds[*j * *p + *j];
	double Dsij   = Ds[*j * *p + *i];
//	vector<double> Dsee = { Dsii, Dsij, Dsij, Dsjj };
	vector<double> Dsee( 4 );
	Dsee[0] = Dsii; 
	Dsee[1] = Dsij; 
	Dsee[2] = Dsij; 
	Dsee[3] = Dsjj; 
	
	int two = 2;
	vector<int> e( 2 );
	e[0] = *i;
	e[1] = *j;

	vector<double> Kcopy( *p * *p ); // double Kcopy[*p * *p];
	copyMatrix( K, &Kcopy[0], p );
 // printMatrix( K, p );	
	if ( G[*j * *p + *i] == 0 )
	{
		// F <- K12 %*% solve(K[-e, -e]) %*% t(K12) 
		vector<double> F( two * two ); // double F[two * two];
		K121output( K, &F[0], &e[0], p );
		//printMatrix( K, p );
		// K[e, e]	
		vector<double> Kee( two * two ); // double Kee[two * two];
		subMatrix( K, &Kee[0], &e[0], &two, p );

		// a <- K[e, e] - F	
		vector<double> a( two * two ); // double a[two * two];	
		minusMatrix( &Kee[0], &F[0], &a[0], &two );
		
		// sig <- sqrt( a[1, 1] / Dsjj )
		double sig;
		sig = sqrt( a[0] / Dsjj );

		// mu = - ( Dsij * a[1, 1] ) / Dsjj
		double mu;
		mu = - ( Dsij * a[0] ) / Dsjj;
		
		// u <- rnorm(1, mu, sig)
		double u;
		u = rnorm( mu, sig );
		
		// v <- rgamma(1, shape = bstar / 2, scale = 2 / Ds[j,j])
		double v;
		v = rgamma( *bstar / 2, 2 / Dsjj );
		
		// K[i,j] <- u + F[1,2]
		Kcopy[*j * *p + *i] = u + F[2];
		// K[j,i] <- u + F[1,2]
		Kcopy[*i * *p + *j] = u + F[2];		
		// K[j,j] <- v + u ^ 2 / a[1,1] + F[2,2]
		Kcopy[*j * *p + *j] = v + pow( u, 2 ) / a[0] + F[3];	
	}
	
	// # (i,j) = 0
	// K0 <- K
	vector<double> K0( *p * *p ); // double K0[*p * *p];
	copyMatrix( &Kcopy[0], &K0[0], p );

	// K0[i, j] <- 0
	K0[*j * *p + *i] = 0;
	// K0[j, i] <- 0
	K0[*i * *p + *j] = 0;
	
	// K0_ij22  <- K_12 %*% solve( K0[-j, -j] ) %*% t(K_12)
	double K0_ij22;
	K111output( &K0[0], &K0_ij22, j, p );

	// K0_ij = diag( c( K[i, i], K0_ij22 ) ) 
	vector<double> K0_ij( 4 );   // = { Kcopy[*i * *p + *i], 0.0, 0.0, K0_ij22 };
	K0_ij[0] = Kcopy[*i * *p + *i];
	K0_ij[1] = 0.0;
	K0_ij[2] = 0.0;
	K0_ij[3] = K0_ij22;

	// # (i,j) = 1
	// K_12  <- K[e, -e]
	// K1_ij <- K_12 %*% solve( K[-e, -e] ) %*% t(K_12) 
	vector<double> K1_ij( two * two ); // double K1_ij[two * two];
	K121output( &Kcopy[0], &K1_ij[0], &e[0], p );

	// a11 <- K[i, i] - K1_ij[1, 1]
	double a11 = Kcopy[*i * *p + *i] - K1_ij[0];
    
// sumDiagAB( double A[], double B[], double C[], double *sumDiag, int *p )
	double sumDiag;
	sumDiagAB( &Dsee[0], &K0_ij[0], &K1_ij[0], &sumDiag, &two );

//   log_Hij = ( log(Dsjj) - log(a11) + ( Dsii - Dsij ^ 2 / Dsjj ) * a11 -
//		        sum( Dsee * ( K0_ij - K1_ij ) ) ) / 2  
//	rate <- Ti[i, i] * Ti[j, j] *
//		    exp( lgamma((b + nustar) / 2) - lgamma((b + nustar - 1) / 2) 
//		    + (1 / 2) * ( log( Ds[j, j] ) - log( a11 ) )
//		    + ((Ds[i, i] - Ds[i, j] ^ 2 / Ds[j, j]) * a11) / 2
//		    - sum( Ds[e, e] * (K0_ij - K1_ij) ) / 2 )

// nustar = sum(A[i, ])
// sumRow ( double A[], double *sumRowi, int *i, int *p )
	int nustar;
	sumRow( G, &nustar, i, p );
	nustar = nustar / 2;

	*HijApprox = sqrt(2.) * Ti[*i * *p + *i] * Ti[*j * *p + *j] *
			   exp( lgamma( ( *b + nustar ) / 2 ) - lgamma( ( *b + nustar - 1 ) / 2 )
	           + ( log(Dsjj) - log(a11) + ( Dsii - pow( Dsij, 2 ) / Dsjj ) * a11 - sumDiag ) / 2 ) ;

	// if ( A[i,j] == 0 ) rate <- 1 / rate 
	if ( G[*j * *p + *i] == 0 ) *HijApprox = 1 / *HijApprox;
}

// Colculating all the birth and death rates
void ratesMatrixApprox( double K[], int G[], double rates[], int *b, int *bstar, double Ti[], double Ds[], int *p )
{
	int i, j;
	double logHij;
	
//	for ( i = 0; i < p * p; i++ ) rates[i] = 0.0;
	for ( i = 0; i < *p; i++ )
		for ( j = i + 1; j < *p; j++ )
		{
			// logHijApprox( double K[], double G[], double *HijApprox, int *i, int *j, int *b, int *bstar, double Ti[], double Ds[], int *p )
			logHijApprox( K, G, &logHij, &i, &j, b, bstar, Ti, Ds, p );
			rates[j * *p + i] = logHij;
		}
}

/////////////////////////////////////////////
void bdmcmcApprox( int *iter, int *burnin, int G[], double Ti[], double Ts[], double K[], int *p, 
			 string allGraphs[], double allWeights[], double Ksum[], 
			 string sampleGraphs[], double graphWeights[], int *sizeSampleG,
			 int lastGraph[], double lastK[],
			 int *b, int *bstar, double Ds[] )
{
	int g;
	//string indG;
	int thisOne;

	int selectedEdge[2];
	
	vector<double> rates( *p * *p, 0.0 ); // double rates[*p * *p];
//	for ( int i = 0; i < *p * *p; i++ )		rates[i] = 0.0;	
	
	for ( g = 0; g < *iter; g++ )
	{
		if ( ( g + 1 ) % 1000 == 0 ) Rprintf( "  Iteration  %d          \n", g + 1 );
		//	cout << "   Iteration " << g + 1 << "\n";
		
		// computing birth and death rates
		// ratesMatrix( double K[], double K_prop[], double G[], double rates[], int *b, int *bstar, double D[], double Ds[], int *p )
// ratesMatrixApprox( double K[], double G[], double rates[], int *b, int *bstar, double Ti[], double Ds[], int *p )
		ratesMatrixApprox( K, G, &rates[0], b, bstar, Ti, Ds, p );

		// indG        <- paste( G[upper.tri(G)], collapse = '' )
		// double G[], string *stringG, int *p
		adjToString( G, &allGraphs[g], p );
		
		//all.G       <- c( all.G, indG )
		//allGraphs[g] = indG;
		// allWeights <- c( allWeights, 1 / sum(rates) )
		// double A[], double *sumUpper, int *p
		sumUpperMatrix( &rates[0], &allWeights[g], p );
		allWeights[g] = 1 / allWeights[g];
		
		if ( g > *burnin )
		{
			//Ksum <- Ksum + K
			// A := A + B
			// double A[], double B[], int *p 
			sumMatrix( Ksum, K, p );
			
			// wh   <- which( sample.G == indG )
			// whichOne( string sampleGraphs[], string *indG, int *thisOne, int *sizeSampleG )
			thisOne = *iter;
			whichOne( sampleGraphs, &allGraphs[g], &thisOne, sizeSampleG );
			
			if ( ( thisOne == *iter ) or ( sizeSampleG == 0 ) )
			{
				sampleGraphs[*sizeSampleG] = allGraphs[g];
				graphWeights[*sizeSampleG] = allWeights[g];
				(*sizeSampleG)++;				
			} else {
				// graphWeights[wh] <- graphWeights[wh] + 1 / sum(rates)
				graphWeights[thisOne] += allWeights[g];
			}
		}

		// To select new graph
		//edge    <- which( rates == max(rates) )[1]
		selectEdge( &rates[0], selectedEdge, p );

		// G[edge] <- 1 - G[edge]
		G[selectedEdge[1] * *p + selectedEdge[0]] = 1 - G[selectedEdge[1] * *p + selectedEdge[0]];
		G[selectedEdge[0] * *p + selectedEdge[1]] = 1 - G[selectedEdge[0] * *p + selectedEdge[1]];

		// K <- rgwish.exact(G = G + t(G), b = bstar, T = Ts, p = p)
		rgwish( G, Ts, K, bstar, p );
	}
	// For last graph and its precision matrix
	// copyMatrix( G, lastGraph, p );
	for ( int g = 0; g < *p * *p; g++ ) lastGraph[g] = G[g];
	copyMatrix( K, lastK, p );			
}
    
////////////////////////////////////////////////////////////////////////////////
// bdmcmc algoirthm with exact value of normalizing constant for D = I_p
// ********************* NEW WORK *****************************************
////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
// For bdmcmcApprox algorithm: computing birth or death rate of element (i,j)
void logHijExact1( double K[], int G[], double *HijExact1, int *i, int *j, int *b, int *bstar, double Ds[], int *p )
{
	double Dsii = Ds[*i * *p + *i];
	double Dsjj = Ds[*j * *p + *j];
	double Dsij = Ds[*j * *p + *i];
//	vector<double> Dsee = { Dsii, Dsij, Dsij, Dsjj };
	vector<double> Dsee( 4 );
	Dsee[0] = Dsii; 
	Dsee[1] = Dsij; 
	Dsee[2] = Dsij; 
	Dsee[3] = Dsjj; 
	
	int two = 2;
	vector<int> e( 2 );
	e[0] = *i;
	e[1] = *j;

	vector<double> Kcopy( *p * *p ); // double Kcopy[*p * *p];
	copyMatrix( K, &Kcopy[0], p );
 // printMatrix( K, p );	
	if ( G[*j * *p + *i] == 0 )
	{
		// F <- K12 %*% solve(K[-e, -e]) %*% t(K12) 
		vector<double> F( two * two ); // double F[two * two];
		K121output( K, &F[0], &e[0], p );
		//printMatrix( K, p );
		// K[e, e]	
		vector<double> Kee( two * two ); // double Kee[two * two];
		subMatrix( K, &Kee[0], &e[0], &two, p );

		// a <- K[e, e] - F	
		vector<double> a( two * two ); // double a[two * two];	
		minusMatrix( &Kee[0], &F[0], &a[0], &two );
		
		// sig <- sqrt( a[1, 1] / Dsjj )
		double sig;
		sig = sqrt( a[0] / Dsjj );

		// mu = - ( Dsij * a[1, 1] ) / Dsjj
		double mu;
		mu = - ( Dsij * a[0] ) / Dsjj;
		
		// u <- rnorm(1, mu, sig)
		double u;
		u = rnorm( mu, sig );
		
		// v <- rgamma(1, shape = bstar / 2, scale = 2 / Ds[j,j])
		double v;
		v = rgamma( *bstar / 2, 2 / Dsjj );
		
		// K[i,j] <- u + F[1,2]
		Kcopy[*j * *p + *i] = u + F[2];
		// K[j,i] <- u + F[1,2]
		Kcopy[*i * *p + *j] = u + F[2];		
		// K[j,j] <- v + u ^ 2 / a[1,1] + F[2,2]
		Kcopy[*j * *p + *j] = v + pow( u, 2 ) / a[0] + F[3];	
	}
	
	// # (i,j) = 0
	// K0 <- K
	vector<double> K0( *p * *p ); // double K0[*p * *p];
	copyMatrix( &Kcopy[0], &K0[0], p );

	// K0[i, j] <- 0
	K0[*j * *p + *i] = 0;
	// K0[j, i] <- 0
	K0[*i * *p + *j] = 0;
	
	// K0_ij22  <- K_12 %*% solve( K0[-j, -j] ) %*% t(K_12)
	double K0_ij22;
	K111output( &K0[0], &K0_ij22, j, p );

	// K0_ij = diag( c( K[i, i], K0_ij22 ) ) 
	vector<double> K0_ij( 4 );   // = { Kcopy[*i * *p + *i], 0.0, 0.0, K0_ij22 };
	K0_ij[0] = Kcopy[*i * *p + *i];
	K0_ij[1] = 0.0;
	K0_ij[2] = 0.0;
	K0_ij[3] = K0_ij22;

	// # (i,j) = 1
	// K_12  <- K[e, -e]
	// K1_ij <- K_12 %*% solve( K[-e, -e] ) %*% t(K_12) 
	vector<double> K1_ij( two * two ); // double K1_ij[two * two];
	K121output( &Kcopy[0], &K1_ij[0], &e[0], p );

	// a11 <- K[i, i] - K1_ij[1, 1]
	double a11 = Kcopy[*i * *p + *i] - K1_ij[0];
    
// sumDiagAB( double A[], double B[], double C[], double *sumDiag, int *p )
	double sumDiag;
	sumDiagAB( &Dsee[0], &K0_ij[0], &K1_ij[0], &sumDiag, &two );

// nustar = sum(A[i, ])
// sumRow ( double A[], double *sumRowi, int *i, int *p )
//	int nustar;
//	sumRow( G, &nustar, i, p );
//	nustar = nustar / 2;

//	An = A
//	An[e] <- 1 - An[e]
//	Af     = An + t(An)
//	nustar = sum( Af[,i] * Af[,j] )
	int nustar = 0;
	for ( int k = 0; k < *p; k++ )
		nustar += G[*i * *p + k] * G[*j * *p + k];

	*HijExact1 = sqrt(2.) *
			   exp( lgamma( ( *b + nustar + 1 ) / 2 ) - lgamma( ( *b + nustar ) / 2 )
	           + ( log(Dsjj) - log(a11) + ( Dsii - pow( Dsij, 2 ) / Dsjj ) * a11 - sumDiag ) / 2 ) ;

	// if ( A[i,j] == 0 ) rate <- 1 / rate 
	if ( G[*j * *p + *i] == 0 ) *HijExact1 = 1 / *HijExact1;
}
   
// Colculating all the birth and death rates
void ratesMatrixExact1( double K[], int G[], double rates[], int *b, int *bstar, double Ds[], int *p )
{
	int i, j;
	double logHij;
	
//	for ( i = 0; i < p * p; i++ ) rates[i] = 0.0;
	for ( i = 0; i < *p; i++ )
		for ( j = i + 1; j < *p; j++ )
		{
			// logHijExact1( double K[], int G[], double *HijExact1, int *i, int *j, int *b, int *bstar, double Ds[], int *p )
			logHijExact1( K, G, &logHij, &i, &j, b, bstar, Ds, p );
			rates[j * *p + i] = logHij;
		}
}

////////////////////////////////////////////////////////////////////////////////
void bdmcmcExact( int *iter, int *burnin, int G[], double Ts[], double K[], int *p, 
			 string allGraphs[], double allWeights[], double Ksum[], 
			 string sampleGraphs[], double graphWeights[], int *sizeSampleG,
			 int lastGraph[], double lastK[],
			 int *b, int *bstar, double Ds[] )
{
	int g;
	//string indG;
	int thisOne;

	int selectedEdge[2];
	
	vector<double> rates( *p * *p, 0.0 ); // double rates[*p * *p];
//	for ( int i = 0; i < *p * *p; i++ )		rates[i] = 0.0;	
	
	for ( g = 0; g < *iter; g++ )
	{
		if ( ( g + 1 ) % 1000 == 0 ) Rprintf( "  Iteration  %d          \n", g + 1 );
		//	cout << "   Iteration " << g + 1 << "\n";
		
		// computing birth and death rates
		// ratesMatrixExact1( double K[], int G[], double rates[], int *b, int *bstar, double Ds[], int *p )
		ratesMatrixExact1( K, G, &rates[0], b, bstar, Ds, p );

		// indG        <- paste( G[upper.tri(G)], collapse = '' )
		// double G[], string *stringG, int *p
		adjToString( G, &allGraphs[g], p );
		
		//all.G       <- c( all.G, indG )
		//allGraphs[g] = indG;
		// allWeights <- c( allWeights, 1 / sum(rates) )
		// double A[], double *sumUpper, int *p
		sumUpperMatrix( &rates[0], &allWeights[g], p );
		allWeights[g] = 1 / allWeights[g];
		
		if ( g > *burnin )
		{
			//Ksum <- Ksum + K
			// A := A + B
			// double A[], double B[], int *p 
			sumMatrix( Ksum, K, p );
			
			// wh   <- which( sample.G == indG )
			// whichOne( string sampleGraphs[], string *indG, int *thisOne, int *sizeSampleG )
			thisOne = *iter;
			whichOne( sampleGraphs, &allGraphs[g], &thisOne, sizeSampleG );
			
			if ( ( thisOne == *iter ) or ( sizeSampleG == 0 ) )
			{
				sampleGraphs[*sizeSampleG] = allGraphs[g];
				graphWeights[*sizeSampleG] = allWeights[g];
				(*sizeSampleG)++;				
			} else {
				// graphWeights[wh] <- graphWeights[wh] + 1 / sum(rates)
				graphWeights[thisOne] += allWeights[g];
			}
		}

		// To select new graph
		//edge    <- which( rates == max(rates) )[1]
		selectEdge( &rates[0], selectedEdge, p );

		// G[edge] <- 1 - G[edge]
		G[selectedEdge[1] * *p + selectedEdge[0]] = 1 - G[selectedEdge[1] * *p + selectedEdge[0]];
		G[selectedEdge[0] * *p + selectedEdge[1]] = 1 - G[selectedEdge[0] * *p + selectedEdge[1]];

		// K <- rgwish.exact(G = G + t(G), b = bstar, T = Ts, p = p)
		rgwish( G, Ts, K, bstar, p );
	}
	// For last graph and its precision matrix
	// copyMatrix( G, lastGraph, p );
	for ( int g = 0; g < *p * *p; g++ ) lastGraph[g] = G[g];
	copyMatrix( K, lastK, p );			
}
    
////////////////////////////////////////////////////////////////////////////////
// Copula Gaussian graphical models 
//********* with NEW idea of exact normalizing constant ***********************
////////////////////////////////////////////////////////////////////////////////
void bdmcmcCopula( int *iter, int *burnin, int G[], double Ts[], double K[], int *p, 
			 double Z[], int R[], int *n,
			 string allGraphs[], double allWeights[], double Ksum[], 
			 string sampleGraphs[], double graphWeights[], int *sizeSampleG,
			 int lastGraph[], double lastK[],
			 int *b, int *bstar, double D[], double Ds[] )
{
	int g;
	int thisOne;

	int selectedEdge[2];
	
	vector<double> rates( *p * *p, 0.0 ); // double rates[*p * *p];
//	for ( int i = 0; i < *p * *p; i++ )		rates[i] = 0.0;	
	
	for ( g = 0; g < *iter; g++ )
	{
		if ( ( g + 1 ) % 1000 == 0 ) Rprintf( "  Iteration  %d          \n", g + 1 );
		//	cout << "   Iteration " << g + 1 << "\n";

		// |------- First step: copula 
		// here we will use a copula function
		// getDs( double K[], double Z[], int R[], double D[], double Ds[], int *n, int *p )
		getDs( K, Z, R, D, Ds, n, p );

	    // Ts = chol( solve(Ds) )
		// getTs( double Ds[], double Ts[], int *p )
		getTs( Ds, Ts, p );
		
		// computing birth and death rates
		// ratesMatrixExact1( double K[], int G[], double rates[], int *b, int *bstar, double Ds[], int *p )
		ratesMatrixExact1( K, G, &rates[0], b, bstar, Ds, p );

		// indG        <- paste( G[upper.tri(G)], collapse = '' )
		// double G[], string *stringG, int *p
		adjToString( G, &allGraphs[g], p );
		
		//all.G       <- c( all.G, indG )
		//allGraphs[g] = indG;
		// allWeights <- c( allWeights, 1 / sum(rates) )
		// double A[], double *sumUpper, int *p
		sumUpperMatrix( &rates[0], &allWeights[g], p );
		allWeights[g] = 1 / allWeights[g];
		
		if ( g > *burnin )
		{
			//Ksum <- Ksum + K
			// A := A + B
			// double A[], double B[], int *p 
			sumMatrix( Ksum, K, p );
			
			// wh   <- which( sample.G == indG )
			// whichOne( string sampleGraphs[], string *indG, int *thisOne, int *sizeSampleG )
			thisOne = *iter;
			whichOne( sampleGraphs, &allGraphs[g], &thisOne, sizeSampleG );
			
			if ( ( thisOne == *iter ) or ( sizeSampleG == 0 ) )
			{
				sampleGraphs[*sizeSampleG] = allGraphs[g];
				graphWeights[*sizeSampleG] = allWeights[g];
				(*sizeSampleG)++;				
			} else {
				// graphWeights[wh] <- graphWeights[wh] + 1 / sum(rates)
				graphWeights[thisOne] += allWeights[g];
			}
		}

		// To select new graph
		//edge    <- which( rates == max(rates) )[1]
		selectEdge( &rates[0], selectedEdge, p );

		// G[edge] <- 1 - G[edge]
		G[selectedEdge[1] * *p + selectedEdge[0]] = 1 - G[selectedEdge[1] * *p + selectedEdge[0]];
		G[selectedEdge[0] * *p + selectedEdge[1]] = 1 - G[selectedEdge[0] * *p + selectedEdge[1]];

		// K <- rgwish.exact(G = G + t(G), b = bstar, T = Ts, p = p)
		rgwish( G, Ts, K, bstar, p );
	}
	// For last graph and its precision matrix
	// copyMatrix( G, lastGraph, p );
	for ( int g = 0; g < *p * *p; g++ ) lastGraph[g] = G[g];
	copyMatrix( K, lastK, p );	
}

////////////////////////////////////////////////////////////////////////////////   
// copula approach for data with missing values
//********* with NEW idea of exact normalizing constant ***********************
///////////////////////////////////////////////////////////////////////////////
void bdmcmcCopulaNA( int *iter, int *burnin, int G[], double Ts[], double K[], int *p, 
			 double Z[], int R[], int *n,
			 string allGraphs[], double allWeights[], double Ksum[], 
			 string sampleGraphs[], double graphWeights[], int *sizeSampleG,
			 int lastGraph[], double lastK[],
			 int *b, int *bstar, double D[], double Ds[] )
{
	int g;
	int thisOne;

	int selectedEdge[2];
	
	vector<double> rates( *p * *p, 0.0 ); // double rates[*p * *p];
//	for ( int i = 0; i < *p * *p; i++ )		rates[i] = 0.0;	
	
	for ( g = 0; g < *iter; g++ )
	{
		if ( ( g + 1 ) % 1000 == 0 ) Rprintf( "  Iteration  %d          \n", g + 1 );
		//	cout << "   Iteration " << g + 1 << "\n";

		// |------- First step: copula 
		// here we will use a copula function
		// getDs( double K[], double Z[], int R[], double D[], double Ds[], int *n, int *p )
		getDsNA( K, Z, R, D, Ds, n, p );

	    // Ts = chol( solve(Ds) )
		// getTs( double Ds[], double Ts[], int *p )
		getTs( Ds, Ts, p );
		
		// computing birth and death rates
		// ratesMatrixExact1( double K[], int G[], double rates[], int *b, int *bstar, double Ds[], int *p )
		ratesMatrixExact1( K, G, &rates[0], b, bstar, Ds, p );

		// indG        <- paste( G[upper.tri(G)], collapse = '' )
		// double G[], string *stringG, int *p
		adjToString( G, &allGraphs[g], p );
		
		//all.G       <- c( all.G, indG )
		//allGraphs[g] = indG;
		// allWeights <- c( allWeights, 1 / sum(rates) )
		// double A[], double *sumUpper, int *p
		sumUpperMatrix( &rates[0], &allWeights[g], p );
		allWeights[g] = 1 / allWeights[g];
		
		if ( g > *burnin )
		{
			//Ksum <- Ksum + K
			// A := A + B
			// double A[], double B[], int *p 
			sumMatrix( Ksum, K, p );
			
			// wh   <- which( sample.G == indG )
			// whichOne( string sampleGraphs[], string *indG, int *thisOne, int *sizeSampleG )
			thisOne = *iter;
			whichOne( sampleGraphs, &allGraphs[g], &thisOne, sizeSampleG );
			
			if ( ( thisOne == *iter ) or ( sizeSampleG == 0 ) )
			{
				sampleGraphs[*sizeSampleG] = allGraphs[g];
				graphWeights[*sizeSampleG] = allWeights[g];
				(*sizeSampleG)++;				
			} else {
				// graphWeights[wh] <- graphWeights[wh] + 1 / sum(rates)
				graphWeights[thisOne] += allWeights[g];
			}
		}

		// To select new graph
		//edge    <- which( rates == max(rates) )[1]
		selectEdge( &rates[0], selectedEdge, p );

		// G[edge] <- 1 - G[edge]
		G[selectedEdge[1] * *p + selectedEdge[0]] = 1 - G[selectedEdge[1] * *p + selectedEdge[0]];
		G[selectedEdge[0] * *p + selectedEdge[1]] = 1 - G[selectedEdge[0] * *p + selectedEdge[1]];

		// K <- rgwish.exact(G = G + t(G), b = bstar, T = Ts, p = p)
		rgwish( G, Ts, K, bstar, p );
	}
	// For last graph and its precision matrix
	// copyMatrix( G, lastGraph, p );
	for ( int g = 0; g < *p * *p; g++ ) lastGraph[g] = G[g];
	copyMatrix( K, lastK, p );	
}
   
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// For generating scale-free graphs: matrix G (p x p) is an adjacency matrix
void scaleFree( int *G, int *p )
{
    int i, j;
    int p0 = 2;
    double randomValue;
    vector<int> size_a( *p ); // int size_a[*p];
    int tmp;
    int total;

    // srand( TRUE );

    for( i = 0; i < p0 - 1; i++ )
    {
        G[i * *p + i + 1]   = 1;
        G[(i + 1) * *p + i] = 1;
    }
        
    for( i = 0; i < p0; i++ )
        size_a[i] = 2;
    
    for( i = p0; i < *p; i++ )
        size_a[i] = 0;
    
    total = 2 * p0;
    
    GetRNGstate();
    for( i = p0; i < *p; i++ )
    {
        // randomValue = (double) total * rand() / RAND_MAX;
        randomValue = (double) total * runif( 0, 1 );
        tmp         = 0;
        j           = 0;
        
        while ( tmp < randomValue && j < i ) 
        {
            tmp += size_a[j];
            j++;
        }
        
        j = j - 1;
        G[i * *p + j] = 1;
        G[j * *p + i] = 1;
        total += 2;
        size_a[j]++;
        size_a[i]++;
    }
	PutRNGstate();
}

////////////////////////////////////////////////////////////////////////////////
    
    
} // exturn "C"
















