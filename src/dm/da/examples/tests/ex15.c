/*$Id: ex15.c,v 1.2 2000/06/01 22:38:25 bsmith Exp bsmith $*/

static char help[] = "Tests DA interpolation\n\n";

#include "petscda.h"
#include "petscsys.h"

#undef __FUNC__
#define __FUNC__ "main"
int main(int argc,char **argv)
{
  int       M = 14,ierr,dof = 1,s = 1,ratio = 2,dim = 1;
  DA        da_c,da_f;
  Vec       v_c,v_f;
  Mat       I;
  Scalar    one = 1.0;
 
  PetscInitialize(&argc,&argv,(char*)0,help);

  ierr = OptionsGetInt(PETSC_NULL,"-dim",&dim,PETSC_NULL);CHKERRA(ierr);
  ierr = OptionsGetInt(PETSC_NULL,"-M",&M,PETSC_NULL);CHKERRA(ierr);
  ierr = OptionsGetInt(PETSC_NULL,"-stencil_width",&s,PETSC_NULL);CHKERRA(ierr);
  ierr = OptionsGetInt(PETSC_NULL,"-ratio",&ratio,PETSC_NULL);CHKERRA(ierr);
  ierr = OptionsGetInt(PETSC_NULL,"-dof",&dof,PETSC_NULL);CHKERRA(ierr);
    
  /* Set up the array */ 
  if (dim == 1) {
    ierr = DACreate1d(PETSC_COMM_WORLD,DA_NONPERIODIC,M,dof,s,PETSC_NULL,&da_c);CHKERRA(ierr);
    M    = ratio*(M-1) + 1;
    ierr = DACreate1d(PETSC_COMM_WORLD,DA_NONPERIODIC,M,dof,s,PETSC_NULL,&da_f);CHKERRA(ierr);
  } else if (dim == 2) {
    ierr = DACreate2d(PETSC_COMM_WORLD,DA_NONPERIODIC,DA_STENCIL_BOX,M,M,PETSC_DECIDE,PETSC_DECIDE,dof,s,PETSC_NULL,PETSC_NULL,&da_c);CHKERRA(ierr);
    M    = ratio*(M-1) + 1;
    ierr = DACreate2d(PETSC_COMM_WORLD,DA_NONPERIODIC,DA_STENCIL_BOX,M,M,PETSC_DECIDE,PETSC_DECIDE,dof,s,PETSC_NULL,PETSC_NULL,&da_f);CHKERRA(ierr);
  } else if (dim == 3) {
    ierr = DACreate3d(PETSC_COMM_WORLD,DA_NONPERIODIC,DA_STENCIL_BOX,M,M,M,PETSC_DECIDE,PETSC_DECIDE,PETSC_DECIDE,dof,s,PETSC_NULL,PETSC_NULL,PETSC_NULL,&da_c);CHKERRA(ierr);
    M    = ratio*(M-1) + 1;
    ierr = DACreate3d(PETSC_COMM_WORLD,DA_NONPERIODIC,DA_STENCIL_BOX,M,M,M,PETSC_DECIDE,PETSC_DECIDE,PETSC_DECIDE,dof,s,PETSC_NULL,PETSC_NULL,PETSC_NULL,&da_f);CHKERRA(ierr);
  }

  ierr = DACreateGlobalVector(da_c,&v_c);CHKERRA(ierr);
  ierr = DACreateGlobalVector(da_f,&v_f);CHKERRA(ierr);

  ierr = VecSet(&one,v_c);CHKERRQ(ierr);
  ierr = DAGetInterpolation(da_c,da_f,&I,PETSC_NULL);CHKERRQ(ierr);
  ierr = MatMult(I,v_c,v_f);CHKERRQ(ierr);
  ierr = VecView(v_f,VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  ierr = MatMultTranspose(I,v_f,v_c);CHKERRQ(ierr);
  ierr = VecView(v_c,VIEWER_STDOUT_WORLD);CHKERRQ(ierr);

  ierr = MatDestroy(I);CHKERRQ(ierr);
  ierr = VecDestroy(v_c);CHKERRA(ierr);
  ierr = DADestroy(da_c);CHKERRA(ierr);
  ierr = VecDestroy(v_f);CHKERRA(ierr);
  ierr = DADestroy(da_f);CHKERRA(ierr);
  PetscFinalize();
  return 0;
}
 



