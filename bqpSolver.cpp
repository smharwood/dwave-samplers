#include <vector>
#include "bqpSolver.h"

using std::vector;

long bqpSolver_tabooSearch(
    BQP *bqp,
    int *starting,
    long startingObjective,
    int tt,
    long long ZCoeff,
    long long timeLimitInMilliSecs,
    const bqpSolver_Callback *callback
) {
    bqp->restartNum++;/*added to record more statistics.*/
    clock_t startTime = clock();
    int i, k, j;
    int globalMinFound;
    long /*objectiveChangeCtr = 0,*/ localSearchCtr = 0;
    long long iter = 0;
    int bestK;
    long localMinCost, cost = 0;
    long prevCost;
    vector<int> u_tabu(bqp->nVars);
    int *taboo = u_tabu.data();
    vector<int> u_sol(bqp->nVars);
    int *solution = u_sol.data();
    int tabooTenure = (20 < (int)(bqp->nVars / 4.0))? 20 : (int)(bqp->nVars / 4.0);
    long long maxIter = (500000 > ZCoeff * (long long)bqp->nVars)? 500000 : ZCoeff * (long long)bqp->nVars;
    vector<long> u_change(bqp->nVars);
    long *changeInObjective = u_change.data(), change;
    double timeLimit = (((double)timeLimitInMilliSecs * (double)CLOCKS_PER_SEC) / (1000.0));

    vector<int> u_tieList(bqp->nVars);
    int numTies = 0, *tieList = u_tieList.data();

    if (tt > 0) {
      tabooTenure = tt;
    }

    for(i = 0; i < bqp->nVars; i++) {
        taboo[i] = 0;
        solution[i] = starting[i];
        bqp->solution[i] = starting[i];
        changeInObjective[i] = bqpUtil_getChangeInObjective(bqp, starting, i);
    }
    bqp->solutionQuality = startingObjective;
    prevCost = bqp->solutionQuality;
    while(iter < maxIter && (clock() - startTime) < timeLimit) {
        bqp->iterNum++; /*added to record more statistics.*/
        localMinCost = LARGE_NUMBER;
        bestK = -1;
        globalMinFound = 0;
        numTies = 0;
        bqp->evalNum += bqp->nVars; /*added to record more statistics.*/
        for(k = 0; k < bqp->nVars; k++) {
            if(taboo[k] != 0) {
                continue;
            }
            iter++;
            cost = prevCost + changeInObjective[k];
            if(cost < bqp->solutionQuality) {
                bestK = k;
                globalMinFound = 1;
                break;
            }
            if(cost < localMinCost) {
                bestK = k;
                localMinCost = cost;
                tieList[0] = k;
                numTies = 1;
            }
            else if(cost == localMinCost) {
                tieList[numTies] = k; numTies++;
            }
        }
        if (!globalMinFound && numTies > 1) {
            bestK = numTies * (double)(rand())/((double)(RAND_MAX)+1);
            bestK = tieList[bestK];
        }
        for(i = 0; i < bqp->nVars; i++) {
            if(taboo[i] > 0) {
                taboo[i] = taboo[i] - 1;
            }
        }
        if(bestK == -1) {
            continue;
        }
        solution[bestK] = 1 - solution[bestK];
        prevCost = localMinCost;
        for(i = 0; i < bestK; i++) {
            change = bqp->Q[i][bestK];
            if(change) {
                changeInObjective[i] += (solution[i] ^ solution[bestK])? change : -change;
            }
        }
        for(i = bestK + 1; i < bqp->nVars; i++) {
            change = bqp->Q[bestK][i];
            if(change) {
                changeInObjective[i] += (solution[i] ^ solution[bestK])? change : -change;
            }
        }
        changeInObjective[bestK] = -changeInObjective[bestK];
        taboo[bestK] = tabooTenure;
        if(globalMinFound == 1) {
            localSearchCtr++;
            bqpSolver_localSearchInternal(bqp, solution, cost, changeInObjective);
            for(i = bqp->nVars; i--;) {
                solution[i] = bqp->solution[i];
            }
            prevCost = bqp->solutionQuality;
            iter += bqp->nIterations;
            bqp->nIterations = iter;

            if (callback != NULL) {
              callback->func(callback, bqp);
            }

            if (bqp->solutionQuality <= bqp->upperBound) timeLimit = 0;
        }
    }
    return bqp->solutionQuality;
}

long bqpSolver_localSearchInternal(BQP *bqp, int *starting, long startingObjective, long *changeInObjective) {
    int i, j;
    long long iter = 0;
    long objective, change;
    int improved;
    for(i = 0; i < bqp->nVars; i++) {
        bqp->solution[i] = starting[i];
    }
    bqp->solutionQuality = startingObjective;
    do {
        improved = 0;
        for(i = 0; i < bqp->nVars; i++, iter++) {
            bqp->evalNum++; /*added to record more statistics.*/
            if(changeInObjective[i] < 0) {
                bqp->solution[i] = 1 - bqp->solution[i];
                bqp->solutionQuality = bqp->solutionQuality + changeInObjective[i];
                improved = 1;
                changeInObjective[i] = -changeInObjective[i];
                for(j = 0; j < bqp->nVars; j++) {
                    change = bqp->Q[i][j] + bqp->Q[j][i];
                    if(change && (j ^ i)) {
                        changeInObjective[j] += (bqp->solution[j] ^ bqp->solution[i])? change : -change;
                    }
                }
            }
        }
    } while(improved);
    bqp->nIterations = iter;
    return bqp->solutionQuality;
}

long bqpSolver_localSearch(BQP *bqp, int *starting) {
    int i;
    vector<long> u_change(bqp->nVars);
    long *changeInObjective = u_change.data();
    for(i = 0; i < bqp->nVars; i++) {
        changeInObjective[i] = bqpUtil_getChangeInObjective(bqp, starting, i);
    }
    bqpSolver_localSearchInternal(bqp, starting, bqpUtil_getObjective(bqp, starting), changeInObjective);
    return bqp->solutionQuality;
}

void bqpSolver_selectVariables(BQP *bqp, int n, vector<vector<long> > &C, int *I) {
    int i, ctr;
    vector<long> u_d(bqp->nVars);
    long *d = u_d.data();
    vector<int> u_sel(bqp->nVars);
    int *selected = u_sel.data();
    int selectedVar = 0;
    vector<double> u_e(bqp->nVars);
    double *e = u_e.data();
    vector<double> u_prob(bqp->nVars);
    double *prob = u_prob.data();
    double selectedProb, prevProb, sumE;
    int allDsEqual;
    long dmin, dmax;
    for(i = 0; i < bqp->nVars; i++) {
        selected[i] = 0;
        d[i] = C[i][i];
    }
    for(ctr = 0; ctr < n; ctr++) {
        dmin = LARGE_NUMBER;
        dmax = -LARGE_NUMBER;
        allDsEqual = 1;
        for(i = 0; i < bqp->nVars; i++) {
            if(selected[i] == 1) {
                continue;
            }
            if(d[i] > dmax) {
                dmax = d[i];
            }
            if(d[i] < dmin) {
                dmin = d[i];
            }
        }
        if(dmin == dmax) {
            for(i = 0; i < bqp->nVars; i++) {
                if(selected[i] == 1) {
                    continue;
                }
                e[i] = 1;
            }
        }
        else {
            for(i = 0; i < bqp->nVars; i++) {
                if(selected[i] == 1) {
                    continue;
                }
                if(d[i] <= 0 && dmin < 0) {
                    e[i] = 1 - (double)d[i] / (double)dmin;
                }
                else if(d[i] == dmin && dmin == 0) {
                    e[i] = 0;
                }
                else {
                    e[i] = 1 + LAMBDA * ((double)d[i] / (float)dmax);
                }
            }
        }
        for(i = 0, sumE = 0, prevProb = 0; i < bqp->nVars; i++) {
            if(selected[i] == 1) {
                continue;
            }
            prob[i] = prevProb + e[i];
            prevProb = prob[i];
            sumE = sumE + e[i];
        }
        if(sumE == 0) {
            sumE = 1;
            for(i = 0; i < bqp->nVars; i++) {
                if(selected[i] == 0) {
                    if(prob[i] == 0) {
                        prob[i] = 1;
                    }
                    else {
                        printf("ERROR: probability is non-zero and yet the sum is zero\n");
                    }
                }
            }
        }
        selectedProb = (double)rand() / ((double)RAND_MAX + 1);
        selectedVar = -1;
        for(i = 0; i < bqp->nVars; i++) {
            if(selected[i] == 1) {
                continue;
            }
            if(selectedProb <= prob[i] / sumE) {
                selectedVar = i;
                break;
            }
        }
        if(selectedVar < 0 || selectedVar > bqp->nVars - 1) {
            printf("ERROR!!!\n");
        }
        I[ctr] = selectedVar;
        selected[selectedVar] = 1;
        for(i = 0; i < bqp->nVars; i++) {
            if(selected[i] == 0) {
                d[i] = d[i] + C[i][selectedVar];
            }
        }
    }
}

void bqpSolver_steepestAscent(int *solution, BQP *bqp, vector<vector<long> > &C, int *I, int n) {
    int i, j, ctr;
    int idI, idJ, r, v = 0;
    vector<long> u_h1(bqp->nVars);
    vector<long> u_h2(bqp->nVars);
    vector<long> u_q1(bqp->nVars);
    vector<long> u_q2(bqp->nVars);
    long *h1 = u_h1.data();
    long *h2 = u_h2.data();
    long *q1 = u_q1.data();
    long *q2 = u_q2.data();
    vector<int> u_visited(bqp->nVars);
    int *visited = u_visited.data();
    long V1, V2;
    for(i = 0; i < bqp->nVars; i++) {
        visited[i] = 0;
        solution[i] = 0;
    }
    for(i = 0; i < n; i++) {
        idI = I[i];
        h1[idI] = C[idI][idI];
        h2[idI] = 0;
        for(j = 0; j < n; j++) {
            idJ = I[j];
            if(idJ != idI) {
                h2[idI] = h2[idI] + (C[idI][idJ]);
            }
        }
        h2[idI] = h2[idI] * 1;
    }
    for(ctr = 0; ctr < n; ctr++) {
        V1 = -LARGE_NUMBER;
        V2 = -LARGE_NUMBER;
        for(i = 0; i < n; i++) {
            idI = I[i];
            if(visited[idI] == 1) {
                continue;
            }
            q1[idI] = 2 * h1[idI] + h2[idI];
            q2[idI] = h1[idI];
            if(q1[idI] > 0 || (q1[idI] == 0 && q2[idI] >= 0)) {
                r = 1;
            }
            else {
                r = 0;
                q1[idI] = -q1[idI];
                q2[idI] = -q2[idI];
            }
            if(q1[idI] > V1 ||(q1[idI] == V1 && q2[idI] > V2)) {
                V1 = q1[idI];
                V2 = q2[idI];
                j = idI;
                v = r;
            }
        }
        solution[j] = v;
        visited[j] = 1;
        for(i = 0; i < n; i++) {
            idI = I[i];
            if(visited[idI] == 1) {
                continue;
            }
            h2[idI] = h2[idI] - 1 * (C[idI][j]);
            if(v == 1) {
                h1[idI] = h1[idI] + 1 * (C[idI][j]);
            }
        }
    }
}

void bqpSolver_computeC(vector<vector<long> > &C, BQP *bqp, int *solution) {
    int i, j;
    for(i = 0; i < bqp->nVars; i++) {
        C[i][i] = -bqp->Q[i][i];
        for(j = i + 1; j < bqp->nVars; j++) {
            if(solution[j] == 1) {
                C[i][i] += -(bqp->Q[i][j]);
            }
            C[i][j] = (solution[i] == solution[j])? -bqp->Q[i][j] : bqp->Q[i][j];
            C[j][i] = C[i][j];
        }
        C[i][i] = (solution[i] == 1)? -C[i][i] : C[i][i];
    }
}

long bqpSolver_multiStartTabooSearch(
    BQP *bqp,
    long long timeLimitInMilliSecs,
    int numStarts,
    int tabuTenure,
    const int *initSolution,
    const bqpSolver_Callback *callback
) {

    clock_t startTime = clock();
    int i;
    long iter;
    vector<vector<long> > C(bqp->nVars);
    vector<int> u_I(bqp->nVars);
    int *I = u_I.data();
    vector<int> u_sol(bqp->nVars);
    int *solution = u_sol.data();
    vector<int> u_bestSol(bqp->nVars);
    int *bestSolution = u_bestSol.data();
    int bestSolutionQuality;
    int n;
    int Z1Coeff = (bqp->nVars <= 500)? 10000 : 25000;
    int Z2Coeff = (bqp->nVars <= 500)? 2500 : 10000;
    for(i = 0; i < bqp->nVars; i++) {
        C[i].resize(bqp->nVars);
    }

    bqpUtil_convertBQPToUpperTriangular(bqp);
    bqpUtil_initBQPSolution(bqp, initSolution);

    bqpSolver_tabooSearch(bqp, bqp->solution.data(), bqp->solutionQuality, tabuTenure, Z1Coeff, timeLimitInMilliSecs, callback);
    bestSolutionQuality = bqp->solutionQuality;
    for(i = 0; i < bqp->nVars; i++) {
        bestSolution[i] = bqp->solution[i];
    }

    for(iter = 0; iter < numStarts && (clock() - startTime) < (((double)timeLimitInMilliSecs * (double)CLOCKS_PER_SEC) / (1000.0)); iter++) {
        if (bqp->solutionQuality <= bqp->upperBound) timeLimitInMilliSecs = 0;
        n = (10 > (int)(ALPHA * bqp->nVars))? 10 : (int)(ALPHA * bqp->nVars);
        if(n > bqp->nVars) {
            n = bqp->nVars;
        }
        bqpSolver_computeC(C, bqp, bqp->solution.data());
        bqpSolver_selectVariables(bqp, n, C, I);
        bqpSolver_steepestAscent(solution, bqp, C, I, n);
        for(i = 0; i < n; i++) {
            if(solution[I[i]] == 1) {
                bqp->solution[I[i]] = 1 - bqp->solution[I[i]];
            }
        }
        bqp->solutionQuality = bqpUtil_getObjective(bqp, bqp->solution.data());
        bqpSolver_tabooSearch(bqp, bqp->solution.data(), bqp->solutionQuality, tabuTenure, Z2Coeff, timeLimitInMilliSecs - (((clock() - startTime) * 1000.0) / (double)CLOCKS_PER_SEC), callback);
        if(bestSolutionQuality > bqp->solutionQuality) {
            bestSolutionQuality = bqp->solutionQuality;
            for(i = 0; i < bqp->nVars; i++) {
                bestSolution[i] = bqp->solution[i];
            }
        }
        if (callback != NULL) {
            callback->func(callback, bqp);
        }
    }
    bqp->solutionQuality = bestSolutionQuality;
    for(i = 0; i < bqp->nVars; i++) {
        bqp->solution[i] = bestSolution[i];
    }
    return bqp->solutionQuality;
}

int bqpSolver_naiveSearch(BQP *bqp) {
    int i;
    unsigned long long num, grayNum;
    long cost, prevCost, minCost;
    int flippedBit;
    vector<int> u_sol(bqp->nVars);
    int *solution = u_sol.data();
    vector<int> u_preSol(bqp->nVars);
    int *prevSolution = u_preSol.data();
    if(bqp->nVars > 64) {
        bqpUtil_randomizeBQPSolution(bqp);
        return bqp->solutionQuality;
    }
    for(i = 0; i < bqp->nVars; i++) {
        prevSolution[i] = 0;
        bqp->solution[i] = 0;
    }
    bqp->nIterations = (long)pow(2, bqp->nVars);
    prevCost = 0;
    minCost = 0;
    for(num = 1; num < bqp->nIterations; num++) {
        grayNum = num ^ (num >> 1);
        for(i = 0; i < bqp->nVars; i++) {
            solution[i] = 0;
        }
        for(i = 0; grayNum != 0 && i < bqp->nVars; i++) {
            solution[i] = (int)(grayNum & 1);
            grayNum = grayNum >> 1;
        }
        flippedBit = -1;
        for(i = 0; i < bqp->nVars; i++) {
            if(solution[i] != prevSolution[i]) {
                flippedBit = i;
                break;
            }
        }
        cost = prevCost + bqpUtil_getChangeInObjective(bqp, prevSolution, flippedBit);
        prevCost = cost;
        prevSolution[flippedBit] = solution[flippedBit];

        if(minCost >= cost) {
            minCost = cost;
            for(i = 0; i < bqp->nVars; i++) {
                bqp->solution[i] = solution[i];
            }
            bqp->solutionQuality = minCost;
        }
    }
    return bqp->solutionQuality;
}

long bqpSolver_restrictedLocalSearchInternal(BQP *bqp, int *starting, int *restricted, long startingObjective, long *changeInObjective) {
    int i, j;
    long long iter = 0;
    long objective, change;
    int improved;
    for(i = 0; i < bqp->nVars; i++) {
        bqp->solution[i] = starting[i];
    }
    bqp->solutionQuality = startingObjective;
    do {
        improved = 0;
        for(i = 0; i < bqp->nVars; i++, iter++) {
            if(restricted[i] == 1) {
                continue;
            }
            objective = bqp->solutionQuality + changeInObjective[i];
            if(objective < bqp->solutionQuality) {
                bqp->solution[i] = 1 - bqp->solution[i];
                bqp->solutionQuality = objective;
                improved = 1;
                changeInObjective[i] = -changeInObjective[i];
                for(j = 0; j < bqp->nVars; j++) {
                    change = bqp->Q[i][j] + bqp->Q[j][i];
                    if(change && (j ^ i)) {
                        changeInObjective[j] += (bqp->solution[j] ^ bqp->solution[i])? change : -change;
                    }
                }
            }
        }
    } while(improved);
    bqp->nIterations = iter;
    return bqp->solutionQuality;
}

long bqpSolver_restrictedLocalSearch(BQP *bqp, int *starting, int *restricted) {
    int i;
    vector<long> u_change(bqp->nVars);
    long *changeInObjective = u_change.data();
    for(i = 0; i < bqp->nVars; i++) {
        changeInObjective[i] = bqpUtil_getChangeInObjective(bqp, starting, i);
    }
    bqpSolver_restrictedLocalSearchInternal(bqp, starting, restricted, bqpUtil_getObjective(bqp, starting), changeInObjective);
    return bqp->solutionQuality;
}





