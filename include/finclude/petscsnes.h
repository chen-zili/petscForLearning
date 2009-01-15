!
!  Include file for Fortran use of the SNES package in PETSc
!
#include "finclude/petscsnesdef.h"

!
!  Convergence flags
!
      PetscEnum SNES_CONVERGED_FNORM_ABS
      PetscEnum SNES_CONVERGED_FNORM_RELATIVE
      PetscEnum SNES_CONVERGED_PNORM_RELATIVE
      PetscEnum SNES_CONVERGED_ITS
      PetscEnum SNES_CONVERGED_TR_DELTA

      PetscEnum SNES_DIVERGED_FUNCTION_DOMAIN
      PetscEnum SNES_DIVERGED_FUNCTION_COUNT
      PetscEnum SNES_DIVERGED_LINEAR_SOLVE
      PetscEnum SNES_DIVERGED_FNORM_NAN
      PetscEnum SNES_DIVERGED_MAX_IT
      PetscEnum SNES_DIVERGED_LS_FAILURE
      PetscEnum SNES_DIVERGED_LOCAL_MIN
      PetscEnum SNES_CONVERGED_ITERATING
   
      parameter (SNES_CONVERGED_FNORM_ABS         =  2)
      parameter (SNES_CONVERGED_FNORM_RELATIVE    =  3)
      parameter (SNES_CONVERGED_PNORM_RELATIVE    =  4)
      parameter (SNES_CONVERGED_ITS               =  5)
      parameter (SNES_CONVERGED_TR_DELTA          =  7)

      parameter (SNES_DIVERGED_FUNCTION_DOMAIN    = -1)
      parameter (SNES_DIVERGED_FUNCTION_COUNT     = -2)  
      parameter (SNES_DIVERGED_LINEAR_SOLVE       = -3)  
      parameter (SNES_DIVERGED_FNORM_NAN          = -4) 
      parameter (SNES_DIVERGED_MAX_IT             = -5)
      parameter (SNES_DIVERGED_LS_FAILURE         = -6)
      parameter (SNES_DIVERGED_LOCAL_MIN          = -8)
      parameter (SNES_CONVERGED_ITERATING         =  0)
     
!
!  Some PETSc fortran functions that the user might pass as arguments
!
      external SNESDEFAULTCOMPUTEJACOBIAN
      external SNESDEFAULTCOMPUTEJACOBIANCOLOR
      external SNESMONITORDEFAULT
      external SNESMONITORLG
      external SNESMONITORSOLUTION
      external SNESMONITORSOLUTIONUPDATE

!PETSC_DEC_ATTRIBUTES(SNESDEFAULTCOMPUTEJACOBIAN,'_SNESDEFAULTCOMPUTEJACOBIAN')
!PETSC_DEC_ATTRIBUTES(SNESDEFAULTCOMPUTEJACOBIANCOLOR,'_SNESDEFAULTCOMPUTEJACOBIANCOLOR')
!PETSC_DEC_ATTRIBUTES(SNESMONITORDEFAULT,'_SNESMONITORDEFAULT')
!PETSC_DEC_ATTRIBUTES(SNESMONITORLG,'_SNESMONITORLG')
!PETSC_DEC_ATTRIBUTES(SNESMONITORSOLUTION,'_SNESMONITORSOLUTION')
!PETSC_DEC_ATTRIBUTES(SNESMONITORSOLUTIONUPDATE,'_SNESMONITORSOLUTIONUPDATE')

      external SNESDEFAULTCONVERGED
      external SNESSKIPCONVERGED

!PETSC_DEC_ATTRIBUTES(SNESDEFAULTCONVERGED,'_SNESDEFAULTCONVERGED')
!PETSC_DEC_ATTRIBUTES(SNESSKIPCONVERGED,'_SNESSKIPCONVERGED')

      external SNESLINESEARCHCUBIC
      external SNESLINESEARCHQUADRATIC
      external SNESLINESEARCHNO
      external SNESLINESEARCHNONORMS

!PETSC_DEC_ATTRIBUTES(SNESLINESEARCHCUBIC,'_SNESLINESEARCHCUBIC')
!PETSC_DEC_ATTRIBUTES(SNESLINESEARCHQUADRATIC,'_SNESLINESEARCHQUADRATIC')
!PETSC_DEC_ATTRIBUTES(SNESLINESEARCHNO,'_SNESLINESEARCHNO')
!PETSC_DEC_ATTRIBUTES(SNESLINESEARCHNONORMS,'_SNESLINESEARCHNONORMS')

      external SNESDAFORMFUNCTION
      external SNESDACOMPUTEJACOBIANWITHADIFOR
      external SNESDACOMPUTEJACOBIAN

!PETSC_DEC_ATTRIBUTES(SNESDAFORMFUNCTION,'_SNESDAFORMFUNCTION')
!PETSC_DEC_ATTRIBUTES(SNESDACOMPUTEJACOBIANWITHADIFOR,'_SNESDACOMPUTEJACOBIANWITHADIFOR')
!PETSC_DEC_ATTRIBUTES(SNESDACOMPUTEJACOBIAN,'_SNESDACOMPUTEJACOBIAN')
!
!  End of Fortran include file for the SNES package in PETSc

