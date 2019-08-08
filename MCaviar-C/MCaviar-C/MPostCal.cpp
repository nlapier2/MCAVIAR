#include <vector>
#include <algorithm>
#include <set>
#include <iostream>
#include <armadillo>
#include <iomanip>
#include <vector>
#include <math.h>

#include "MUtil.h"
#include "MPostCal.h"

using namespace arma;


void printGSLPrint(mat &A, int row, int col) {
    for(int i = 0; i < row; i++) {
        for(int j = 0; j < col; j++)
            printf("%g ", A(i, j));
        printf("\n");
    }
}

string MPostCal::convertConfig2String(int * config, int size) {
    string result = "0";
    for(int i = 0; i < size; i++)
        if(config[i]==1)
            result+= "_" + convertInt(i);
    return result;
}

mat* MPostCal::construct_diagC(int * configure) {
    /*
     construct sigma_C by the kronecker product in paper, it is mn by mn. the variance for vec(lambdaC)|vec(C)
     :param configure the causal status vector of 0 and 1
     :return diagC is the variance matrix for (lamdaC|C)
     */
    mat Identity_M = mat(num_of_studies, num_of_studies, fill::eye);
    mat Matrix_of_1 = mat(num_of_studies, num_of_studies);
    Matrix_of_1.fill(1);
    mat temp1 = t_squared * Identity_M + s_squared * Matrix_of_1;
    mat temp2 = mat(snpCount, snpCount, fill::zeros);
    for(int i = 0; i < snpCount; i++) {
        if (configure[i] == 1)
            temp2[i][i] = 1;
    }
    mat diagC = kron(temp1, temp2);
    return diagC;
}

double MPostCal::likelihood(int * configure, double * stat, double NCP) {
    /*
     compute likelihood of each configuration by Woodbury
     :param configure the causal status vector of 0 and 1
     :param stat the z-score of each snp
     :param NCP the non-centrality param, set to higher of 5.2 or the highest z_score of all snps in all studies
     :return likelihood of the configuration
     */
    int causalCount = 0;
    double matDet = 0;
    double res    = 0;
    
    for(int i = 0; i < snpCount; i++)
        causalCount += configure[i];
    if(causalCount == 0){
        mat tmpResultMatrixNM = statMatrixtTran * invSigmaMatrix;
        mat tmpResultMatrixNN = tmpResultMatrix1N * statMatrix;
        
        res = tmpResultMatrixNN(0,0);
        matDet = sigmaDet;
        return (-res/2-sqrt(abs(matDet)));
    }
    
    mat sigmaC = construct_diagC(configure);
    
    int index_C = 0;
    // U is kn by mn matrix of columns corresponding to causal SNP in sigmaC
    mat U(causalCount * num_of_studies, snpCount * num_of_studies, fill::zeros);
    for (int i = 0; i < snpCount * num_of_studies; i++) {
        if (configure[i] == 1) {
            for (int j = 0; j < snpCount * num_of_studies; j++)
                U(index_C, j) = sigmaC(i, j);
            index_C ++;
        }
    }
    
    index_C = 0;
    // V is mn by kn matrix of rows corresponding to causal SNP in sigma
    mat V(causalCount * num_of_studies, snpCount * num_of_studies, fill::zeros);
    mat sigmaMatrixTran = trans(sigmaMatrix);
    for (int i = 0; i < snpCount * num_of_studies; i++) {
        if (configure[i] == 1) {
            for (int j = 0; j < snpCount * num_of_studies; j++)
                V(index_C, j) = sigmaMatrixTran(i, j);
            index_C ++;
        }
    }
    V = trans(V);
    
    // UV = SigmaC * Sigma (kn by kn)
    mat UV(causalCount * num_of_studies, causalCount * num_of_studies, fill::zeros);
    UV = U * V;
    
    mat I_AA   = mat(snpCount, snpCount, fill::eye);
    mat tmp_CC = mat(causalCount, causalCount, fill::eye)+ UV;
    matDet = det(tmp_CC) * sigmaDet;
    
    mat tmp_AA = invSigmaMatrix - (invSigmaMatrix * V) * pinv(tmp_CC) * U ;
    //tmp_AA     = invSigmaMatrix * tmp_AA;
    mat tmpResultMatrix1N = statMatrixtTran * tmp_AA;
    mat tmpResultMatrix11 = tmpResultMatrix1N * statMatrix;
    res = tmpResultMatrix11(0,0);
    
    if(matDet==0) {
        cout << "Error the matrix is singular and we fail to fix it." << endl;
        exit(0);
    }
    /*
     We compute the log of -res/2-log(det) to see if it is too big or not.
     In the case it is too big we just make it a MAX value.
     */
    double tmplogDet = log(sqrt(abs(matDet)));
    double tmpFinalRes = -res/2 - tmplogDet;
    return tmpFinalRes;
}

int MPostCal::nextBinary(int * data, int size) {
    int i = 0;
    int total_one = 0;
    int index = size-1;
    int one_countinus_in_end = 0;
    
    while(index >= 0 && data[index] == 1) {
        index = index - 1;
        one_countinus_in_end = one_countinus_in_end + 1;
    }
    if(index >= 0) {
        while(index >= 0 && data[index] == 0) {
            index = index - 1;
        }
    }
    if(index == -1) {
        while(i <  one_countinus_in_end+1 && i < size) {
            data[i] = 1;
            i=i+1;
        }
        i = 0;
        while(i < size-one_countinus_in_end-1) {
            data[i+one_countinus_in_end+1] = 0;
            i=i+1;
        }
    }
    else if(one_countinus_in_end == 0) {
        data[index] = 0;
        data[index+1] = 1;
    } else {
        data[index] = 0;
        while(i < one_countinus_in_end + 1) {
            data[i+index+1] = 1;
            if(i+index+1 >= size)
                printf("ERROR3 %d\n", i+index+1);
            i=i+1;
        }
        i = 0;
        while(i < size - index - one_countinus_in_end - 2) {
            data[i+index+one_countinus_in_end+2] = 0;
            if(i+index+one_countinus_in_end+2 >= size) {
                printf("ERROR4 %d\n", i+index+one_countinus_in_end+2);
            }
            i=i+1;
        }
    }
    i = 0;
    total_one = 0;
    for(i = 0; i < size; i++)
        if(data[i] == 1)
            total_one = total_one + 1;
    
    return(total_one);
}

double MPostCal::computeTotalLikelihood(vector<double>* stat, double NCP) {
    int num = 0;
    double sumLikelihood = 0;
    double tmp_likelihood = 0;
    long int total_iteration = 0 ;
    int* configure = (int *) malloc (snpCount * sizeof(int *)); // original data
    
    for(long int i = 0; i <= maxCausalSNP; i++)
        total_iteration = total_iteration + nCr(snpCount, i);
    cout << "Max Causal=" << maxCausalSNP << endl;
    
    for(long int i = 0; i < snpCount; i++)
        configure[i] = 0;
    
    int* tempConfigure = configure;
    for (int i = 0; i < num_of_studies - 1; i++){
        tempConfigure = tempConfigure * pow(2, snpCount) + configure;
    }
    
    for(long int i = 0; i < total_iteration; i++) {
        tmp_likelihood = Likelihood(tempConfigure, stat, NCP) + num * log(gamma) + (snpCount-num) * log(1-gamma);
        sumLikelihood = addlogSpace(sumLikelihood, tmp_likelihood);
        for(int j = 0; j < snpCount; j++) {
            for(int k = 0; k < num_of_studies; k++){
                postValues[j] = addlogSpace(postValues[j], tmp_likelihood * configure[j + k * self.snpCount]);
            }
        }
        histValues[num] = addlogSpace(histValues[num], tmp_likelihood);
        num = nextBinary(configure, snpCount);
        tempConfigure = configure;
        for (int m = 0; m < num_of_studies; m++){
            tempConfigure = tempConfigure * pow(2, snpCount) + configure;
        }
        //cout << i << " "  << exp(tmp_likelihood) << endl;
        if(i % 1000 == 0)
            cerr << "\r                                                                 \r" << (double) (i) / (double) total_iteration * 100.0 << "%";
    }
    
    for(int i = 0; i <= maxCausalSNP; i++)
        histValues[i] = exp(histValues[i]-sumLikelihood);
    free(configure);
    return(sumLikelihood);
}

bool MPostCal::validConfigutation(int * configure, char * pcausalSet) {
    for(int i = 0; i < snpCount; i++){
        if(configure[i] == 1 && pcausalSet[i] == '0')
            return false;
    }
    return true;
}

/*
 * This is a auxilary function used to generate all possible causal set that
 * are selected in the p-causal set
 */
void MPostCal::computeALLCausalSetConfiguration(double * stat, double NCP, char * pcausalSet, string outputFileName) {
    int num = 0;
    double sumLikelihood = 0;
    double tmp_likelihood = 0;
    long int total_iteration = 0 ;
    int * configure = (int *) malloc (snpCount * sizeof(int *)); // original data
    
    for(long int i = 0; i <= maxCausalSNP; i++)
        total_iteration = total_iteration + nCr(snpCount, i);
    for(long int i = 0; i < snpCount; i++)
        configure[i] = 0;
    for(long int i = 0; i < total_iteration; i++) {
        if (validConfigutation(configure, pcausalSet)) {
            //log space
            tmp_likelihood = fastLikelihood(configure, stat, NCP) +  num * log(gamma) + (snpCount-num) * log(1-gamma);
            exportVector2File(outputFileName, configure, snpCount);
            export2File(outputFileName, tmp_likelihood);
        }
        num = nextBinary(configure, snpCount);
    }
}

/*
 stat is the z-scpres
 sigma is the correaltion matrix
 G is the map between snp and the gene (snp, gene)
 */
double MPostCal::findOptimalSetGreedy(double * stat, double NCP, char * pcausalSet, int *rank,  double inputRho, string outputFileName) {
    int index = 0;
    double rho = double(0);
    double total_post = double(0);
    
    totalLikeLihoodLOG = computeTotalLikelihood(stat, NCP);
    
    export2File(outputFileName+".log", exp(totalLikeLihoodLOG)); //Output the total likelihood to the log File
    for(int i = 0; i < snpCount; i++)
        total_post = addlogSpace(total_post, postValues[i]);
    printf("Total Likelihood= %e SNP=%d \n", total_post, snpCount);
    
    std::vector<data> items;
    std::set<int>::iterator it;
    //output the poster to files
    for(int i = 0; i < snpCount; i++) {
        //printf("%d==>%e ",i, postValues[i]/total_likelihood);
        items.push_back(data(exp(postValues[i]-total_post), i, 0));
    }
    printf("\n");
    std::sort(items.begin(), items.end(), by_number());
    for(int i = 0; i < snpCount; i++)
        rank[i] = items[i].index1;
    
    for(int i = 0; i < snpCount; i++)
        pcausalSet[i] = '0';
    do{
        rho += exp(postValues[rank[index]]-total_post);
        pcausalSet[rank[index]] = '1';
        printf("%d %e\n", rank[index], rho);
        index++;
    } while( rho < inputRho);
    
    printf("\n");
    return(0);
}