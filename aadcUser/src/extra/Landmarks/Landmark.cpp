#include "Landmark.h"

using namespace cv;

// ----- Default Constructor ---------------------------------------------------
Landmark::Landmark (  )
{
} 
// -----------------------------------------------------------------------------

// ----- Initializing Constructor ----------------------------------------------
Landmark::Landmark ( Vec3d u, Matx33d cov ) :
mean(u),
covariance(cov),
age(0),
fitness(0)
{
} 
// -----------------------------------------------------------------------------

// ----- Destructor ------------------------------------------------------------
Landmark::~Landmark (  )
{

} 
// -----------------------------------------------------------------------------
