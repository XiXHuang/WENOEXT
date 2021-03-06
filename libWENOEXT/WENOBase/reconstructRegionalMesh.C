/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

Author
    Jan Wilhelm Gärtner <jan.gaertner@outlook.de> Copyright (C) 2020

\*---------------------------------------------------------------------------*/

#include "reconstructRegionalMesh.H"


Foam::fileName Foam::reconstructRegionalMesh::localPath
(
    const fvMesh& localMesh,
    const label proci,
    const fileName file
)
{
    // Get total path
    return localMesh.time().path().path()/fileName("processor" + name(proci))/file;
}


Foam::autoPtr<Foam::fvMesh> Foam::reconstructRegionalMesh::reconstruct
(
    const labelList processorList,
    const fvMesh& localMesh
)
{
    word regionName = polyMesh::defaultRegion;
    word regionDir = word::null;

    scalar mergeTol = 1E-7;
    
    label nProcs = processorList.size();
    
    
    // Read point on individual processors to determine merge tolerance
    // (otherwise single cell domains might give problems)
    const boundBox bb = procBounds(processorList,localMesh);
    const scalar mergeDist = mergeTol*bb.mag();

    //Info<< "Overall mesh bounding box  : " << bb << nl
        //<< "Relative tolerance         : " << mergeTol << nl
        //<< "Absolute matching distance : " << mergeDist << nl
        //<< endl;
        
    fvMesh* masterMesh = 
        (
            new fvMesh
            (
                IOobject
                (
                    regionName,
                    localMesh.time().timeName(),
                    localMesh.time(),
                    IOobject::NO_READ,
                    IOobject::NO_WRITE,
                    false
                ),
                #ifdef FOAM_MOVE_CONSTRUCTOR
                    pointField(),
                    faceList(),
                    labelList(),
                    labelList()
                #else
                    xferCopy(pointField()),
                    xferCopy(faceList()),
                    xferCopy(labelList()),
                    xferCopy(labelList())
                #endif
            )
        );

    for (label proci=0; proci<nProcs; proci++)
    {
        #ifdef FULLDEBUG
            Pout << "Reading processor mesh: "<<processorList[proci]
                 <<"  ("<<proci<<" of "<<nProcs-1<<")"<<endl;
        #endif
        
        // Mesh cannot be constructed with boundaries as this calls the 
        // updateMesh() function of coupled processors and causes an MPI error
        // --> Solution construct without boundary
        
        pointField points = readField<point>
        (
            localPath
            (
                localMesh,
                processorList[proci],
                fileName("constant/" + polyMesh::meshSubDir)/fileName("points")
            )
        );

        faceList faces = readFaceList
        (
            localPath
            (
                localMesh,
                processorList[proci],
                fileName("constant/" + polyMesh::meshSubDir)/fileName("faces")
            )
        );
        
        labelList owner = readList<label>
        (
            localPath
            (
                localMesh,
                processorList[proci],
                fileName("constant/" + polyMesh::meshSubDir)/fileName("owner")
            )
        );

        labelList neighbour = readList<label>
        (
            localPath
            (
                localMesh,
                processorList[proci],
                fileName("constant/" + polyMesh::meshSubDir)/fileName("neighbour")
            )
        );
        
            
        fvMesh meshToAdd
        (
            IOobject
            (
                regionName,
                localMesh.time().timeName(),
                localMesh.time(),
                IOobject::NO_READ,
                IOobject::NO_WRITE,
                false
            ),
            #ifdef FOAM_MOVE_CONSTRUCTOR
                std::move(points),
                std::move(faces),
                std::move(owner),
                std::move(neighbour),
            #else
                xferMove(points),
                xferMove(faces),
                xferMove(owner),
                xferMove(neighbour),
            #endif
            false // Do not synchronize --> in polyMesh() bounds_ calls an mpirecv 
        );

        // Now add the boundaries by creating a polyPatch with a type patch 
        // for generic value
        
        // Read polyPatchList
        const polyBoundaryMesh& polyMeshRef = meshToAdd.boundaryMesh();
         autoPtr<ISstream> isPtr = fileHandler().readStream
         (
            const_cast<polyBoundaryMesh&>(polyMeshRef),
            localPath(localMesh,processorList[proci],fileName("constant/polyMesh/boundary")),
            "polyBoundaryMesh"
        );
 
        ISstream& is = isPtr();
 
         PtrList<entry> patchEntries(is);
         
         List<polyPatch*> patches(patchEntries.size());
 
         forAll(patches, patchi)
         {
             patches[patchi] = 
                (polyPatch::New
                (
                   "patch",
                    patchEntries[patchi].keyword(),
                    patchEntries[patchi].dict(),
                    patchi,
                    polyMeshRef
                )).ptr();
         }

        meshToAdd.addPatches(patches,false);

        // Find geometrically shared points/faces.
        autoPtr<faceCoupleInfo> couples
        (
            new faceCoupleInfo
            (
                *masterMesh,
                meshToAdd,
                mergeDist,      // Absolute merging distance
                true            // Matching faces identical
            )
        );

        // Add elements to mesh
        autoPtr<mapAddedPolyMesh> map = add
        (
            *masterMesh,
            meshToAdd,
            couples,
            false // do not synchronize
        );
        
    }
    
    return autoPtr<fvMesh>(masterMesh);
}


Foam::autoPtr<Foam::mapAddedPolyMesh> Foam::reconstructRegionalMesh::add
(
    fvMesh& mesh0,
    const fvMesh& mesh1,
    const faceCoupleInfo& coupleInfo,
    const bool validBoundary
)
{
    // -----------------------------------
    // Function taken from fvMeshAddr::add
    // -----------------------------------
    
    mesh0.clearOut();
    
    // Resulting merged mesh (polyMesh only!)
    autoPtr<mapAddedPolyMesh> mapPtr
    (
        polyMeshAdder::add
        (
            mesh0,
            mesh1,
            coupleInfo,
            validBoundary
        )
    );
    
    return mapPtr;
}


Foam::boundBox Foam::reconstructRegionalMesh::procBounds
(
    const labelList processorList,
    const fvMesh& localMesh
)
{
    boundBox bb = boundBox::invertedBox;

    forAll(processorList, proci)
    {
        pointField points
        (
            readField<point>
            (
                localPath(localMesh,processorList[proci],fileName("constant/"+polyMesh::meshSubDir+"/points"))
            )
        );

        boundBox domainBb(points, false);

        bb.min() = min(bb.min(), domainBb.min());
        bb.max() = max(bb.max(), domainBb.max());
    }

    return bb;
}


void Foam::reconstructRegionalMesh::readHeader(Istream& is)
{
     // Check Istream not already bad
     if (!is.good())
     {
        FatalIOErrorInFunction(is)
             << " Stream is not good"  
             << exit(FatalIOError);
     }
 
     token firstToken(is);
 
     if
     (
         is.good()
      && firstToken.isWord()
      && firstToken.wordToken() == "FoamFile"
     )
     {
         dictionary headerDict(is);
 
         is.version(headerDict.lookup("version"));
         is.format(headerDict.lookup("format"));
         word headerClassName = word(headerDict.lookup("class"));
 
        //if (headerClassName == "faceCompactList")
            //FatalIOErrorInFunction(is)
             //<< " stream failure while "
             //<< " reading file " << is.name() << nl
             //<< "\tFile is in binary format. Mesh has to be saved in ascii format"
             //<< exit(FatalIOError);
 
         const word headerObject(headerDict.lookup("object"));
         
         #ifdef FULLDEBUG
             if (headerObject != headerClassName)
             {
                 IOWarningInFunction(is)
                     << " object renamed from "
                     << headerClassName << " to " << headerObject
                     << " for file " << is.name() << endl;
             }
         #endif
     }
     else
     {
        FatalIOErrorInFunction(is)
             << " stream failure while reading header"
             << " on line " << is.lineNumber()
             << " of file " << is.name()
             << exit(FatalIOError);
     }
 
     // Check stream is still OK
     if (!is.good())
     {
         FatalIOErrorInFunction(is)
             << " stream failure while reading header"
             << " on line " << is.lineNumber()
             << " of file " << is.name()    
             << exit(FatalIOError);
     }

}


Foam::List<Foam::face> Foam::reconstructRegionalMesh::readFaceList
(
    const fileName path
)
{
    // Create an IFStream object
    IFstream is(path);
    readHeader(is);
    
    if (is.format() == IOstream::streamFormat::BINARY)
    {
        List<face> L;
        
        // Read compact
        const labelList start(is);
        const List<label> elems(is);
    
        // Convert
        L.setSize(start.size()-1);
    
        forAll(L, i)
        {
            face& subList = L[i];
    
            label index = start[i];
            subList.setSize(start[i+1] - index);
    
            forAll(subList, j)
            {
                subList[j] = elems[index++];
            }
        }
        
        return L;
    }
    else
        return List<face>(is);
}
