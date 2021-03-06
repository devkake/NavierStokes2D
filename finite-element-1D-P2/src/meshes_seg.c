#include "meshes_seg.h"
#include <fstream>
#include <cmath>
#include <stdexcept>
#include <string>
#include <cassert>
// #include <suitesparse/umfpack.h>
#include <umfpack.h>

int Seg::NbSeg = 0;

Seg::Seg(){
    NbSeg++;
}
Seg::Seg(R1  & point1, R1 & point2) // Création à partir de 2 points
{
    NbSeg++;
    vertices[0] = &point1;
    vertices[1] = &point2;
}

Seg::Seg(const Seg& Scopy)    // Constructeur par copie
{    
    NbSeg++;
    vertices[0] = Scopy[0];
    vertices[1] = Scopy[1];
}

Seg::~Seg()   // Décrémentation de NbTri à la destruction d'un objet
{
    NbSeg--;
}

void Seg::operator=(const Seg& Scopy)  // Opérateur copie
{
    vertices[0] = Scopy[0];
    vertices[1] = Scopy[1];
}
ostream& operator<<(ostream& stream,const Seg& Segment) // Affichage du stream
{
    stream <<"Segment( " << (*Segment.vertices[0]) <<", " << (*Segment.vertices[1]) << " )" <<endl;
    return stream;
}

void Seg::set(int nbVertice, R1 & vertice)
{
    vertices[nbVertice] = &vertice;
}

R1 * Seg::get(int nbVertice) const
{
    assert(nbVertice < nbNodes);
    return vertices[nbVertice];
}

void Seg::show()
{
    cout <<"Segment(";
    vertices[0]->show();
    vertices[1]->show();
    cout<<")"<<endl;
}
void Seg::showRaw()
{
    cout <<"SegmentPointsAddresses("  << vertices[0] <<", " << vertices[1] <<")"<<endl;

}

void Seg::addInteriorPoint(R1& ip)
{
    this->interiorPoints.push_back(&ip);
}

R1* Seg::getInteriorPoint(int i) const
{
    return this->interiorPoints.at(i);
}

R1 * Seg::operator[](int nbVert) const
{
    assert(nbVert < nbNodes);
    return vertices[nbVert];
}
int Seg::Nb()
{
    return NbSeg;
}


vector<R1> loadNodes(char* file)  // Generate Nodes from text file
{
    ifstream meshfile;
    meshfile.open(file);
    R1 point = R1(NAN);
    if(meshfile.is_open()){
    // First line parser
	    int nbVertice, nbSeg, nbBord;
	    meshfile >> nbVertice >> nbSeg >> nbBord;
	//  cout << nbVertice << endl;

	    vector<R1> arrayNodes(nbVertice,point);
	    double xValue = -1.;
	    int boundary;
    // Reading points in the file
	    for(int i = 0; i<nbVertice;i++)
	    {
		    meshfile >> xValue >> boundary;
		    point.set(xValue);
            point.setBoundary( (int) (boundary !=0 ));
		    arrayNodes[i] =  point;
	    }
        meshfile.close();
	    return arrayNodes;
    }
    else
    {
        throw std::runtime_error("Impossible d'ouvrir le fichier: "+( (string) file) );
    }
}



void loadSeg(char* file,vector<R1> & arrayNodes, vector<Seg> &arraySeg,vector<vector<Seg*> > & segInNodes)
{
    ifstream meshfile;
    meshfile.open(file);
    if(!meshfile.is_open())
    {
        throw std::runtime_error("Impossible d'ouvrir le fichier: "+( (string) file) );
    }
    int nbVertice, nbSeg, nbLine, seg1, seg2, boundary, p1, p2;
    // First line parser
    meshfile >> nbVertice >> nbSeg >> nbLine;

    Seg initSeg =  Seg();
    arraySeg.resize(nbSeg,initSeg);
    // Skipping the vertice declaration
    char*  buffer = new char[256];
    meshfile.getline(buffer, 256);
    for(int i=0; i<nbVertice; i++)    
    {
        meshfile.getline(buffer, 256);
    }
    segInNodes.resize(nbVertice);

    // Reading the vertices, linking with the segments
    for(int i =0; i<nbSeg; i++)
    {
        meshfile >> p1 >> p2  >> boundary;
        // Constructing the segment
        arraySeg[i].set(0,arrayNodes[p1-1]);
        arraySeg[i].set(1,arrayNodes[p2-1]);

        // Constructing the array of segment containing p1 and p2
        segInNodes[p1-1].push_back(&arraySeg[i]);
        segInNodes[p2-1].push_back(&arraySeg[i]);

    }
    segInNodes.resize(nbVertice);
    meshfile.close();
    return;
}

Mesh_1D::Mesh_1D(char* filename)
{
    Nodes = loadNodes( filename);
    loadSeg(filename, Nodes, Segments, SegmentContainingNodes);
    
}

void Mesh_1D::make_Stiffness_Matrix()
{
    row_ptr.resize(0);
    col_ind.resize(0);
    value.resize(0);
    // Temporary vectors to store one row, in order to store the 
    vector<double> value_temp(Nodes.size(),NAN);                        // The values are initialized at NAN and become real if the value of the matrix is not zero
    vector<int> col_ind_temp;
    


    int nbNodesInVertices = Seg::nbNodes, indiceOfMatrix=0;
    // int nbNodesInVertices = 2 * Seg::nbNodes - 1, indiceOfMatrix=0;

    double valueLinearForm=NAN;

    R1 * stockNodes;

    row_ptr.push_back(indiceOfMatrix);

    // Go through all the nodes

    for(vector<vector<Seg*> >::iterator itNodes = SegmentContainingNodes.begin() ; itNodes < SegmentContainingNodes.end(); ++itNodes)
    {

        // Go through all vertices that contains the node 
        for(vector<Seg*>::iterator itSeg = (*itNodes).begin(); itSeg < (*itNodes).end(); ++itSeg)
        {
            // Go through all the nodes of the vertice and store the scalar product
            for (int i=0; i<nbNodesInVertices ; i++)
            {   

                stockNodes = (*itSeg)->get(i);                                                                    // Temporary node that is a neighbour of the node (or the node itself) in the vertice (*itSeg)
                valueLinearForm = bilinearForm(&Nodes[itNodes-SegmentContainingNodes.begin()],stockNodes,**itSeg);

                if(isnan(value_temp[stockNodes-&(Nodes[0])]) && fabs(valueLinearForm)>0.000000001 )               // The matrix does not store the zero-values
                {
                    col_ind_temp.push_back( ((int) (stockNodes-&(Nodes[0])) ) );                                  // The NAN check permits to add the column number only once                         
                    value_temp[stockNodes-&(Nodes[0])] = 0;        

                }

                if(fabs(valueLinearForm)>0.000000001)
                {
                    value_temp[stockNodes-&(Nodes[0])]+= bilinearForm(&Nodes[itNodes-SegmentContainingNodes.begin()],stockNodes,**itSeg);
                }

            }
            
        }

        // Exact penalty method
       
        if(Nodes[itNodes-SegmentContainingNodes.begin()].isBoundary())
        {
            value_temp[itNodes-SegmentContainingNodes.begin()]=penalty_coeff;
        }

        // Copying the temporary values to the  real vectors

        for(vector<int>::iterator itIndexNode = col_ind_temp.begin(); itIndexNode < col_ind_temp.end(); ++itIndexNode)
        {
            indiceOfMatrix++;  
            value.push_back(value_temp[*itIndexNode]);
            value_temp[*itIndexNode] =  NAN;            // Setting back the values to NAN
        }
    
        // Reset the temporary values
        col_ind.insert(col_ind.end(),col_ind_temp.begin(),col_ind_temp.end());
        col_ind_temp.resize(0);
        row_ptr.push_back(indiceOfMatrix);

    }
    assert(col_ind.size() ==  value.size());
/*    cout << "Col_ind: " <<col_ind.size() <<endl;
    for(int i = 0; i< col_ind.size(); i++)
    {
        cout << col_ind[i] << " ";
    }
    cout << endl << endl;
    cout << "Value: " << value.size() << endl;
    for(int i = 0; i< value.size(); i++)
    {
        cout << value[i] << " ";
    }
    cout << endl << endl;
        cout << "Row_ptr: "<<  row_ptr.size() << endl;
    for(int i = 0; i< row_ptr.size(); i++)
    {
        cout << row_ptr[i] << " ";
    }
    cout << endl << endl; */
    
}

void Mesh_1D::get_Stiffness_Matrix(vector<double> & value_get,vector<int> & col_ind_get,vector<int> & row_ptr_get)
{
    value_get = value;
    col_ind_get = col_ind;
    row_ptr_get = row_ptr;
}

void Mesh_1D::make_Constant_Vector()
{
    constVect.resize(0);
    constVect.resize(Nodes.size(),0.);
     // Go through all the nodes
    for(vector<vector<Seg*> >::iterator itNodes = SegmentContainingNodes.begin() ; itNodes < SegmentContainingNodes.end(); ++itNodes)
    {
        assert(constVect[itNodes-SegmentContainingNodes.begin()] == 0.);
        if(!Nodes[itNodes-SegmentContainingNodes.begin()].isBoundary())
        {
        // Go through all vertices that contains the node 
            for(vector<Seg*>::iterator itSeg = (*itNodes).begin(); itSeg < (*itNodes).end(); ++itSeg)
            {
                constVect[itNodes-SegmentContainingNodes.begin()] += linearForm(&Nodes[itNodes-SegmentContainingNodes.begin()], **itSeg); 
            }
        }
        else
        {
            // Exact penalty method
            constVect[itNodes-SegmentContainingNodes.begin()]  = limitCondition(&Nodes[itNodes-SegmentContainingNodes.begin()])*penalty_coeff;
        }
    }
         
}

void Mesh_1D::get_Constant_Vector(vector<double> & constVect_get)
{
    constVect_get = constVect;
}
double Orthogonal_Mesh_1D::bilinearForm(R1 * originPoint, R1 *  otherPoint, Seg segment)
{
    if(originPoint == otherPoint)
    {
        return fabs( segment[0]->get()-segment[1]->get() );
    }
    else
    {
        return 0.;
    }
}
double Orthogonal_Mesh_1D::limitCondition(R1* point)
{
    return 0;
}          
double Orthogonal_Mesh_1D::linearForm(R1 * originPoint, Seg segment)
{
        return 0.;
}

double P1_Lapl_Mesh_1D::bilinearForm(R1 * originPoint, R1 *  otherPoint, Seg segment)
{
        double area = fabs( segment[0]->get()-segment[1]->get() );
        double scalar = 0;
        if(originPoint == otherPoint)
        {
            scalar = 1./(area*area);
        }
        else  
        {
            scalar =-1./(area*area);
        }
        
        return  scalar*area;
}

double P1_Lapl_Mesh_1D::linearForm(R1 * originPoint, Seg segment)
{
        double area = fabs( segment[0]->get()-segment[1]->get() );
        double scalar = 1./2.;
        return  scalar*area;
}
double P1_Lapl_Mesh_1D::limitCondition(R1* point)
{
    return 1;
}     


//
// newly added class
// for P2 approximation 
//


P2_Lapl_Mesh_1D::P2_Lapl_Mesh_1D(char* filename) : Mesh_1D(filename)
{
    // Nodes = loadNodes(filename);
    // loadSeg(filename, Nodes,Segments,SegmentContainingNodes);
    this->originalNbNodes = Nodes.size();
    this->newNbNodes = originalNbNodes - 1;
    adaptForP2(); 
}

void P2_Lapl_Mesh_1D::adaptForP2()
{
    for (vector<Seg>::iterator itSeg = this->Segments.begin(); itSeg < this->Segments.end(); ++itSeg)
    {   
        R1 tempR1(0.5 * (*((*itSeg).get(0)) + *((*itSeg).get(1)))); // complicated 
        this->Nodes.push_back(tempR1); // extend Nodes size: originalNbNodes -> originalNbNodes + newNbNodes
        this->SegmentContainingInteriorPoint.push_back(&(*itSeg)); // map: new nodes index -> segment where it is
        (*itSeg).addInteriorPoint(this->Nodes.back());
    }
}

/*
P2_Lapl_Mesh_1D::adaptMeshForP2()
{
    vector<Seg> tempSegs;
    int nodeIndex = 0;
    for (vector<Seg>::iterator itSeg = this->Segments.begin(); itSeg < this->Segments.end(); ++itSeg)
    {   
        R1 tempR1(1. / 2. * (*((*itSeg).get(0)) + *((*itSeg).get(1)))); // complicated 
        this->Nodes.push_back(tempR1);
        tempSegs.push_back(Seg(*((*itSeg).get(0)), this->Nodes.at(this->originalNbNodes + nodeIndex))); // passing R1 reference implicitly 
        tempSegs.push_back(Seg(*((*itSeg).get(1)), this->Nodes.at(this->originalNbNodes + nodeIndex))); // by difinition of Segement class
        nodeIndex += 1; // index rule
    }
    this->Segments.clear()
    std::copy(tempSegs.begin(), tempSegs.end(), std::back_inserter(this->Segments));

    this->SegmentContainingNodes.clear(); // vector<vector<Seg*> >
    for (vector<R1>::iterator itNode = this->Nodes.begin(); itNode < this->Nodes.end(); ++itNode)
    {   
        vector<Seg*> tempSegs;
        int nbNeighborCount = 0;
        for (vector<Seg>::iterator itSeg = this->Segments.begin(); itSeg < this->Segments.end(); ++itSeg)
        {   
            if (compR1(*itNode, *(*(itSeg).get(0))) or compR1(*itNode, *(*(itSeg).get(1))))
            {
                tempSegs.push_back(&(*itSeg));
                nbNeighborCount += 1;
            }
            if (nbNeighborCount >= 2)
            {
                break;
            }
            this->SegmentContainingNodes.push_back(tempSegs);
        }

    }


}
*/

double P2_Lapl_Mesh_1D::bilinearForm(R1* originPoint, R1*  otherPoint, Seg segment)
{
    double area = fabs(segment[0]->get() - segment[1]->get());
    double scalar = 0.;
    if(originPoint == otherPoint)
    {
        scalar = 2. / 15.;
    }
    else  
    {
        scalar = -1. / 30.;
    }
    
    return scalar / area;
}

double P2_Lapl_Mesh_1D::bilinearForm_2(R1* originPoint, R1* otherPoint, Seg segment)
{
    double area = fabs(segment[0]->get() - segment[1]->get());
    double scalar = 0.;
    scalar = 1. / 15.;
    return scalar / area;
}

double P2_Lapl_Mesh_1D::bilinearForm_3(R1* originPoint, R1* otherPoint, Seg segment)
{
    double area = fabs(segment[0]->get() - segment[1]->get());
    double scalar = 0.;
    scalar = 8. / 15.;
    return scalar / area;
}

double P2_Lapl_Mesh_1D::linearForm(R1 * originPoint, Seg segment)
{
        double area = fabs( segment[0]->get()-segment[1]->get() );
        double scalar = 1./2.;
        return  scalar*area;
}
double P2_Lapl_Mesh_1D::limitCondition(R1* point)
{
    return 1;
}     

void P2_Lapl_Mesh_1D::make_Stiffness_Matrix()
{
    row_ptr.resize(0);
    col_ind.resize(0);
    value.resize(0);

    vector<int> col_ind, row_ptr; 
    vector<double> value_temp(this->Nodes.size(), NAN);
    vector<int> col_ind_temp;

    int indiceOfMatrix=0;
    double valueLinearForm=NAN; // initialisation
    R1* stockNode;
    R1* stockInteriorNode;
    Seg* stockSeg;

    row_ptr.push_back(indiceOfMatrix);

    // following iteration is based on the node index
    // because matrix is of size nbNodes * nbNodes
    int indexNode = 0;
    for (vector<R1>::iterator itNode = this->Nodes.begin(); itNode < this->Nodes.end(); ++itNode)
    {  
        if (indexNode < this->originalNbNodes) // CASE 1: for original nodes
        {
            for(vector<Seg*>::iterator itSeg = (this->SegmentContainingNodes.at(indexNode)).begin(); itSeg < (this->SegmentContainingNodes.at(indexNode)).end(); ++itSeg)
            {
                // interaction between original nodes
                for (int i=0; i < 2; i++) // ATTENTION: need to double-count for self overlap calculation (diagonal elements)
                {   
                    stockNode = (*itSeg)->get(i);
                    valueLinearForm = bilinearForm(&(this->Nodes.at(indexNode)), stockNode, **itSeg); // accordance will be checked by "address"
                    if (isnan(value_temp.at(stockNode - &(Nodes.at(0)))) && fabs(valueLinearForm) > 1.0e-10) // frist modification
                    {
                        col_ind_temp.push_back(((int) (stockNode - &(Nodes.at(0))))); // address difference !!!DANGEROUS!!! 
                        value_temp.at(stockNode - &(Nodes.at(0))) = valueLinearForm; 
                    }
                    if(fabs(valueLinearForm) > 1.0e-10) // case double count modification
                    {
                        value_temp.at(stockNode - &(Nodes.at(0))) += valueLinearForm;
                    }
                }

                // interaction between original nodes and new (interior) nodes
                stockInteriorNode = (*itSeg)->getInteriorPoint(0);
                valueLinearForm = bilinearForm_2(&(this->Nodes.at(indexNode)), stockInteriorNode, **itSeg); // to be defined
                col_ind_temp.push_back(((int) (stockInteriorNode - &(Nodes.at(0))))); // address difference !!!DANGEROUS!!! 
                value_temp.at(stockNode - &(Nodes.at(0))) = valueLinearForm;       
            }
        }    
        else // CASE 2: for new nodes, that is, interior points
        {
            stockSeg = this->SegmentContainingInteriorPoint.at(indexNode - this->originalNbNodes); // segment where the point (node) is
            // interaction between new (interior) nodes and original nodes
            for (int i=0; i < 2; i++)
            {   
                stockNode = stockSeg->get(i); // obtain edge node of the segement
                valueLinearForm = bilinearForm_2(&(this->Nodes.at(indexNode)), stockNode, *stockSeg);
                col_ind_temp.push_back(((int) (stockNode - &(Nodes.at(0))))); // address difference !!!DANGEROUS!!! 
                value_temp.at(stockNode - &(Nodes.at(0))) = valueLinearForm; 
            }
            // interaction between new (interior) nodes
            valueLinearForm = bilinearForm_3(&(*itNode), &(*itNode), *stockSeg);
            col_ind_temp.push_back(((int) (&(*itNode) - &(Nodes.at(0))))); // address difference !!!DANGEROUS!!! 
            value_temp.at(&(*itNode) - &(Nodes.at(0))) = valueLinearForm; 
        }
        
        // Exact penalty method
        if(this->Nodes.at(indexNode).isBoundary())
        {
            value_temp.at(indexNode) = penalty_coeff; // huge number
        }

        // following is for a sparse matrix
        // keep only non-zero element

        // Copying the temporary values to the  real vectors
        for(vector<int>::iterator itIndexNode = col_ind_temp.begin(); itIndexNode < col_ind_temp.end(); ++itIndexNode)
        {
            indiceOfMatrix += 1; // count how many there are in a target row
            value.push_back(value_temp.at(*itIndexNode));
            value_temp.at(*itIndexNode) =  NAN; // initialising by NAN for the next iteration
        }
    
        // Reset the temporary values
        col_ind.insert(col_ind.end(), col_ind_temp.begin(), col_ind_temp.end());
        col_ind_temp.clear(); // initialising for the next iteration
        row_ptr.push_back(indiceOfMatrix); // number of non-zero element in a target row

    // increment of node index
    indexNode += 1;

    }

    assert(col_ind.size() ==  value.size());
    
}

//
//
//


void Mesh_1D::solveSystem()
{
    
    int sizeMatrix = Nodes.size();
    int numberNonZero = value.size();
    assert(numberNonZero == col_ind.size());
    if(sizeMatrix == 0)
    {
        throw std::runtime_error("Erreur: tentative de résolution du problème sans avoir chargé de maillage (nbNoeuds = 0).");
    }

    // Getting the CRS Matrix

    if(value.size() == 0 || col_ind.size() == 0 || row_ptr.size() == 0)
    {
        throw std::runtime_error("Erreur: tentative de résolution du problème sans avoir calculé la matrice de rigidité (matrice de rigidité de taille nulle).");
    }


    double * valArray = &value[0];
    int * colArray = &col_ind[0], * rowArray = &row_ptr[0];

    // Getting the constant values of the system

    if(constVect.size() == 0)
    {
        throw std::runtime_error("Erreur: tentative de résolution du problème sans avoir calculé la partie constante du système (vecteur constant de taille nulle).");
    }


    
    double * constArray =  &constVect[0];

    // Array that store the result

    double result[sizeMatrix];

    double *null = ( double * ) NULL;
    void *Numeric;
    int status;
    void *Symbolic;
    for(int i=0; i< numberNonZero; i++)
    {
        cout << valArray[i] << " ";
    }
    cout << endl << endl;
    for(int i=0; i< numberNonZero; i++)
    {
        cout << colArray[i]<< " ";
    }
    cout << endl<< endl << numberNonZero << endl;

    //
    //  Carry out the symbolic factorization.
    //
      status = umfpack_di_symbolic ( sizeMatrix, sizeMatrix, rowArray, colArray, valArray, &Symbolic, null, null );
    //
    //  Use the symbolic factorization to carry out the numeric factorization.
    //
      status = umfpack_di_numeric (rowArray, colArray,valArray, Symbolic, &Numeric, null, null );
    //
    //  Free the memory associated with the symbolic factorization.
    //
      umfpack_di_free_symbolic ( &Symbolic );
    //
    //  Solve the linear system.
    //
      status = umfpack_di_solve ( UMFPACK_A, rowArray, colArray, valArray, result, constArray, Numeric, null, null );
    //
    //  Free the memory associated with the numeric factorization.
    //
      umfpack_di_free_numeric ( &Numeric );
    cout<<endl;
    for(int i=0; i<sizeMatrix; i++)
    {
        cout << Nodes[i].get() << " " << result[i]<< " "<< constArray[i] <<endl;
    }
}

void exportGnuPlot(vector<Seg> inputSeg, string file){
    ofstream plotfile;
    plotfile.open(file.c_str());
    if(!plotfile.is_open())
    {
        throw std::runtime_error("Impossible d'ouvrir le fichier: "+file);
    }
    plotfile << "# Syntax for gnuplot: 'plot '"+file +"' using 1:(0):($2-$1):(0) with vectors'" <<endl;
    plotfile << "# The grid is a list of segment between the points (X0,Y0) and (X1,Y1)" <<endl;   
    plotfile << "# X0 Y0 X1 Y1" <<endl;
    for(vector<Seg>::iterator it = inputSeg.begin(); it != inputSeg.end(); ++it)
    {
        plotfile<< it->get(0)->get() << " ";
        plotfile<< it->get(1)->get()  <<endl;
    }
    plotfile.close();
    
}


     
