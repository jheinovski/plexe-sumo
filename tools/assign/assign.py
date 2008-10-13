"""
@file    assign.py
@author  Yun-Pang.Wang@dlr.de
@date    2007-11-25
@version $Id$

This script is for executing traffic assignment according to the required assignment model.
The incremental assignment model, the C-Logit assignment model and the Lohse assignment model are included in this script. 

Copyright (C) 2008 DLR/TS, Germany
All rights reserved
"""

import math, operator
import elements
from elements import Vertex, Edge, Path, Vehicle
from network import Net

def doIncAssign(vehicles, verbose, iteration, endVertices, start, startVertex, matrixPshort, D, P, AssignedVeh, AssignedTrip, edgeNums, vehID): 
    # matrixPlong and matrixTruck should be added if available.
    for end, endVertex in enumerate(endVertices): 
        if str(startVertex) != str(endVertex) and (matrixPshort[start][end] > 0.0):
        # if matrixPling and the matrixTruck exist, matrixPlong[start][end] > 0.0 or matrixTruck[start][end] > 0.0): should be added.
            helpPath = []
            pathtime = 0.
            pathlength = 0.
        
            vertex = endVertex
            while vertex != startVertex:
                if P[vertex].kind == "real":
                    helpPath.append(P[vertex])
                    if len(helpPath) > edgeNums:
                        print 'len(helpPath) > edgeNums!'
                        print 'startVertex:', startVertex.label
                        print 'endVertex:', endVertex.label
                        foutroutecheck = file('routeCheck.txt','a')
                        helpPath.reverse()
                        for line, link in enumerate(helpPath):
                            if link % 20 == 0:
                                foutroutecheck.write('\n')
                            foutroutecheck.write('%s ' %link)
                        foutroutecheck.close()
                        print 'check the file: routeCheck.txt!'
                        sys.exit()
                    P[vertex].flow += (matrixPshort[start][end]/float(iteration))
                vertex = P[vertex].source
            helpPath.reverse()
            
            # for generating vehicle routes used in SUMO 
            for edge in helpPath[1:-1]: 
                pathlength += edge.length
                pathtime += edge.actualtime
            # the amount of the pathflow, which will be released at this iteration
            pathflow = float(matrixPshort[start][end]/float(iteration))
            if verbose:
                print 'pathflow:', pathflow
            
            AssignedTrip[startVertex][endVertex] += pathflow
            
            vehID = assignVeh(verbose, vehicles, startVertex, endVertex, helpPath, AssignedVeh, AssignedTrip, vehID)

    return vehID
  
# execute the SUE model with the given path set
def doSUEAssign(net, options, startVertices, endVertices, matrixPshort, mtxOverlap, iter, lohse, first): 
    if lohse:
        if options.verbose:
            foutassign = file('lohse_pathSet.txt', 'a')
            foutassign.write('\niter:%s\n' %iter)

    # matrixPlong and matrixTruck should be added if available.
    if options.verbose:
        print 'pathNum in doSUEAssign:', elements.pathNum           
    # calculate the overlapping factors between any two paths of a given OD pair
    for start, startVertex in enumerate(startVertices): 
        for end, endVertex in enumerate(endVertices):
            cumulatedflow = 0.
            pathcount = 0
                        
            if matrixPshort[start][end] > 0. and str(startVertex) != str(endVertex):
                ODPaths = net._paths[startVertex][endVertex]
                
                for path in ODPaths:
                    path.getPathTimeUpdate()
                calCommonalityAndChoiceProb(ODPaths, mtxOverlap, options.alpha, options.gamma, lohse)
                
                # calculate the path choice probabilities and the path flows for the given OD Pair
                for path in ODPaths:
                    pathcount += 1
                    if pathcount < len(ODPaths):
                        path.helpflow = matrixPshort[start][end] * path.choiceprob
                        cumulatedflow += path.helpflow
                        if lohse and options.verbose:
                            foutassign.write('    path:%s\n' % path.label)
                            foutassign.write('    path.choiceprob:%s\n' % path.choiceprob)
                            foutassign.write('    path.helpflow:%s\n' % path.helpflow)
                            foutassign.write('    cumulatedflow:%s\n' % cumulatedflow)
                    else:
                        path.helpflow = matrixPshort[start][end] - cumulatedflow 
                        if lohse and options.verbose:
                            foutassign.write('    last_path.helpflow:%s\n' % path.helpflow)
                    if first and iter == 1:
                        for edge in path.edges:
                            if edge.source.label != edge.target.label:
                                edge.flow += path.helpflow
                    else:
                        for edge in path.edges:
                            if edge.source.label != edge.target.label:
                                edge.helpflow += path.helpflow
    
    # Reset the convergence index for the C-Logit model
    notstable = 0
    stable = False
    # link travel timess and link flows will be updated according to the latest traffic assingment  
    for edge in net._edges.itervalues():                                       
        if edge.source.label != edge.target.label:
            if (first and iter > 1) or (not first):
                exflow = edge.flow
                edge.flow = edge.flow + (1./iter)*(edge.helpflow - edge.flow)
                
                if not lohse:
                    if edge.flow > 0.:
                        if abs(edge.flow-exflow)/edge.flow > options.sueTolerance:
                            notstable += 1
                    elif edge.flow == 0.:
                        if exflow != 0. and (abs(edge.flow-exflow)/exflow > options.sueTolerance):
                            notstable += 1
                    elif edge.flow < 0.:
                        notstable += 1
                        edge.flow = 0.
                else:
                    if edge.flow < 0.:
                        edge.flow = 0.
            
            # reset the edge.helpflow for the next iteration
            edge.helpflow = 0.0                                                
            edge.getActualTravelTime(options, lohse)
 #           if edge.queuetime > 0.:
 #               notstable += 1
    if lohse and options.verbose:
        foutassign.close()
                                                               
    if not lohse and iter > 3:
        if notstable == 0:
            stable = True        
        elif notstable < len(net._edges)*0.05:
            stable = True
            
        if iter > options.maxiteration:
            stable = True
         
    return stable

# calculate the commonality factors in the C-Logit model
def calCommonalityAndChoiceProb(ODPaths, mtxOverlap, alpha, gamma, lohse):
    # initialize the overlapping matrix
    for pathone in ODPaths:
        doCal = False
        if not pathone in mtxOverlap:
            mtxOverlap[pathone]={}
            doCal = True
        for pathtwo in ODPaths:
            if doCal or not pathtwo in mtxOverlap[pathone]:
                if not pathtwo in mtxOverlap[pathone]:
                    mtxOverlap[pathone][pathtwo] = 0.
                    doCal = True
                if not pathtwo in mtxOverlap:
                    mtxOverlap[pathtwo] = {}
                    mtxOverlap[pathtwo][pathone] = 0.
                    doCal = True
                if doCal:
                    for edgeone in pathone.edges:
                        for edgetwo in pathtwo.edges:
                            if edgeone.label == edgetwo.label:
                                mtxOverlap[pathone][pathtwo] += edgeone.length
                    mtxOverlap[pathone][pathtwo] = mtxOverlap[pathone][pathtwo]/1000.
                    mtxOverlap[pathtwo][pathone] = mtxOverlap[pathone][pathtwo]
                doCal = False
    
    if len(ODPaths) > 1:
        for pathone in ODPaths:
            sum_overlap = 0.0 
            lengthOne = pathone.length/1000.
            for pathtwo in ODPaths:
                lengthTwo = pathtwo.length/1000.
                sum_overlap += math.pow(mtxOverlap[pathone][pathtwo]/(math.pow(lengthOne,0.5) * math.pow(lengthTwo,0.5)), gamma)
          
            pathone.commfactor = alpha * math.log(sum_overlap)
        
        if lohse:
            minpath = min(ODPaths, key=operator.attrgetter('pathhelpacttime'))
            minpathcost = minpath.pathhelpacttime + minpath.commfactor
            beta = 12./(1.+ math.exp(0.7 - 0.015 * minpath.pathhelpacttime))
        else:
            theta = getThetaForCLogit(ODPaths)

        for pathone in ODPaths:
            sum_exputility = 0.
            for pathtwo in ODPaths:
                if str(pathone.label) != str(pathtwo.label):
                    if not lohse:
                        sum_exputility += math.exp(theta*(-pathtwo.actpathtime + pathone.actpathtime + pathone.commfactor - pathtwo.commfactor))
                    else:
                        pathonecost = pathone.pathhelpacttime + pathone.commfactor
                        pathtwocost = pathtwo.pathhelpacttime + pathtwo.commfactor
                        sum_exputility += math.exp(-(beta*(pathtwocost/minpathcost -1.))**2.+(beta*(pathonecost/minpathcost -1.))**2.)
            pathone.choiceprob = 1./(1. + sum_exputility)
    else:
        for path in ODPaths:
            path.commfactor = 0.
            path.choiceprob = 1.
            
# calculate the path choice probabilities and the path flows and generate the vehicular data for each OD Pair    
def doSUEVehAssign(net, vehicles, options, counter, matrixPshort, startVertices, endVertices, AssignedVeh, AssignedTrip, mtxOverlap, vehID, lohse):
    if options.verbose:
        if counter == 0:
            foutpath = file('paths.txt', 'w')
            fouterror = file('errors.txt', 'w')
        else:
            foutpath = file('paths.txt', 'a')
            fouterror = file('errors.txt', 'a')
        if lohse:
            foutpath.write('begin the doSUEVehAssign based on the lohse assignment model!')
        else:
            foutpath.write('begin the doSUEVehAssign based on the c-logit model!')
        foutpath.write('the analyzed matrix=%s' %counter)
        
    TotalPath = 0

    for start, startVertex in enumerate(startVertices):
        if options.verbose:
            foutpath.write('\norigin=%s, ' %startVertex)
        for end, endVertex in enumerate(endVertices):
            pathcount = 0
            cumulatedflow = 0.
            if matrixPshort[start][end] > 0. and str(startVertex) != str(endVertex):
                if options.verbose:
                    foutpath.write('destination=%s' %endVertex)
                ODPaths = net._paths[startVertex][endVertex]
                
                for path in ODPaths:
                    TotalPath += 1
                    path.getPathTimeUpdate()
                    if lohse:                      
                        path.pathhelpacttime = path.actpathtime
      
                calCommonalityAndChoiceProb(ODPaths, mtxOverlap, options.alpha, options.gamma, lohse)
        
                for path in ODPaths:
                    pathcount += 1
                    if pathcount < len(ODPaths):
                        path.pathflow = matrixPshort[start][end] * path.choiceprob
                        cumulatedflow += path.pathflow
                    else:
                        path.pathflow = matrixPshort[start][end] - cumulatedflow
                        if path.pathflow < 0.:
                            fouterror.write('*********************** the path flow on the path:%s < 0.!!' %path.label)
                    if options.verbose:
                        foutpath.write('\npathID= %s, path flow=%4.4f, actpathtime=%4.4f, choiceprob=%4.4f, edges=' 
                                        %(path.label, path.pathflow, path.actpathtime, path.choiceprob))
                        for item in path.edges:
                            foutpath.write('%s, ' %(item.label))
                        
                    AssignedTrip[startVertex][endVertex] += path.pathflow
                    edges = path.edges
                    vehID = assignVeh(options.verbose, vehicles, startVertex, endVertex, edges, AssignedVeh, AssignedTrip, vehID)
                if options.verbose:
                    foutpath.write('\n')
    if options.verbose:
        print 'total Number of the used paths for the current matrix:', TotalPath 
        foutpath.write('\ntotal Number of the used paths for the current matrix:%s' %TotalPath)
        foutpath.close()
        fouterror.close()
    return vehID

           
def assignVeh(verbose, vehicles, startVertex, endVertex, edges, AssignedVeh, AssignedTrip, vehID):
    while AssignedVeh[startVertex][endVertex] < int(round(AssignedTrip[startVertex][endVertex])):
        vehID += 1
        newVehicle = Vehicle(str(vehID))
        newVehicle.route = edges
        vehicles.append(newVehicle)
        
        AssignedVeh[startVertex][endVertex] += 1
    if verbose:
        print 'vehID:', vehID
        print 'AssignedTrip[start][end]', AssignedTrip[startVertex][endVertex]
        print 'AssignedVeh[start][end]', AssignedVeh[startVertex][endVertex]
    
    return vehID

def getThetaForCLogit(ODPaths):
    sum = 0.
    diff = 0.
    minpath = min(ODPaths, key=operator.attrgetter('actpathtime'))
    
    for path in ODPaths:
        sum += path.actpathtime
    
    meanpathtime = sum / float(len(ODPaths))
    
    for path in ODPaths:
        diff += (path.actpathtime - meanpathtime)**2.

    sdpathtime = (diff/float(len(ODPaths)))**0.5

    if sdpathtime > 0.04:
        theta = math.pi / (pow(6.,0.5) * sdpathtime * minpath.actpathtime)
    else:
        theta = 1.

    return theta
    
def doLohseStopCheck(net, options, stable, iter, maxIter, foutlog):
    stable = False
    if iter > 1 :                                        # Check if the convergence reaches.
        counts = 0    
        for edge in net._edges.itervalues():
            stop = edge.stopCheck(options)
            if stop: 
                counts += 1
        if counts == len(net._edges):
            stable = True
            foutlog.write('The defined convergence is reached. The number of the required iterations:%s\n' %iter)
        elif counts < int(len(net._edges)*0.05) and iter > 50:
            stable = True
            foutlog.write('The number of the links with convergence is 95% of the total links. The number of executed iterations:%s\n' %iter)

    if iter >= maxIter:
        print 'The max. number of iterations is reached!'
        foutlog.write('The max. number(%s) of iterations is reached!\n' %iter)
        foutlog.write('The number of new routes will be set to 0, since the max. number of iterations is reached.')
        stable = True
        print 'stop?:', stable
        print 'iter_inside:', iter
    return stable