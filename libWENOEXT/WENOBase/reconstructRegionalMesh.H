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

Description
    Reconstruct regional mesh.
    Bases on reconstructParMesh.C of OpenFOAM 

SourceFiles
    reconstructRegionalMesh.C

Author
    Jan Wilhelm Gärtner <jan.gaertner@outlook.de> Copyright (C) 2020

\*---------------------------------------------------------------------------*/

#include "IOobjectList.H"
#include "labelIOList.H"
#include "processorPolyPatch.H"
#include "mapAddedPolyMesh.H"
#include "polyMeshAdder.H"
#include "faceCoupleInfo.H"
#include "fvMeshAdder.H"
#include "polyTopoChange.H"
#include "extrapolatedCalculatedFvPatchFields.H"
#include "IFstream.H"
#include "codeRules.H"

namespace Foam
{

namespace reconstructRegionalMesh
{

    //- Reconsturct mesh depending on processor list
    autoPtr<fvMesh> reconstruct
    (
        const labelList processorList,
        const fvMesh& localMesh
    );
    
    boundBox procBounds
    (
        const labelList processorList,
        const fvMesh& localMesh
    );
    
    fileName localPath
    (
        const fvMesh& localMesh, 
        const label proci,
        const fileName file
    );
    
    void readHeader(Istream& is);
    
    List<face> readFaceList
    (
        const fileName path
    );
    
    template<class Type>
    Field<Type> readField(const fileName path)
    {
        // Create an IFStream object
        IFstream is(path);      
        readHeader(is);
        return Field<Type>(is);
    }
    
    template<class Type>
    List<Type> readList(const fileName path)
    {
        // Create an IFStream object
        IFstream is(path);
        readHeader(is);
        
        return List<Type>(is);
    }
    
    autoPtr<mapAddedPolyMesh> add
    (
        fvMesh& mesh0,
        const fvMesh& mesh1,
        const faceCoupleInfo& coupleInfo,
        const bool validBoundary
    );
    
}

}
