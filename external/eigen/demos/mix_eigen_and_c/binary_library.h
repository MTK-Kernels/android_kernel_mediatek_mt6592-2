// Copyright (C) 2009 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed


#ifdef __cplusplus
extern "C"
{
#endif

  
  
  struct C_MatrixXd {};
  struct C_Map_MatrixXd {};

  
  
  struct C_MatrixXd* MatrixXd_new(int rows, int cols);
  void    MatrixXd_delete     (struct C_MatrixXd *m);
  double* MatrixXd_data       (struct C_MatrixXd *m);
  void    MatrixXd_set_zero   (struct C_MatrixXd *m);
  void    MatrixXd_resize     (struct C_MatrixXd *m, int rows, int cols);
  void    MatrixXd_copy       (struct C_MatrixXd *dst,
                               const struct C_MatrixXd *src);
  void    MatrixXd_copy_map   (struct C_MatrixXd *dst,
                               const struct C_Map_MatrixXd *src);  
  void    MatrixXd_set_coeff  (struct C_MatrixXd *m,
                               int i, int j, double coeff);
  double  MatrixXd_get_coeff  (const struct C_MatrixXd *m,
                               int i, int j);
  void    MatrixXd_print      (const struct C_MatrixXd *m);
  void    MatrixXd_add        (const struct C_MatrixXd *m1,
                               const struct C_MatrixXd *m2,
                               struct C_MatrixXd *result);  
  void    MatrixXd_multiply   (const struct C_MatrixXd *m1,
                               const struct C_MatrixXd *m2,
                               struct C_MatrixXd *result);
  
  
  
  struct C_Map_MatrixXd* Map_MatrixXd_new(double *array, int rows, int cols);
  void   Map_MatrixXd_delete     (struct C_Map_MatrixXd *m);
  void   Map_MatrixXd_set_zero   (struct C_Map_MatrixXd *m);
  void   Map_MatrixXd_copy       (struct C_Map_MatrixXd *dst,
                                  const struct C_Map_MatrixXd *src);
  void   Map_MatrixXd_copy_matrix(struct C_Map_MatrixXd *dst,
                                  const struct C_MatrixXd *src);  
  void   Map_MatrixXd_set_coeff  (struct C_Map_MatrixXd *m,
                                  int i, int j, double coeff);
  double Map_MatrixXd_get_coeff  (const struct C_Map_MatrixXd *m,
                                  int i, int j);
  void   Map_MatrixXd_print      (const struct C_Map_MatrixXd *m);
  void   Map_MatrixXd_add        (const struct C_Map_MatrixXd *m1,
                                  const struct C_Map_MatrixXd *m2,
                                  struct C_Map_MatrixXd *result);  
  void   Map_MatrixXd_multiply   (const struct C_Map_MatrixXd *m1,
                                  const struct C_Map_MatrixXd *m2,
                                  struct C_Map_MatrixXd *result);

#ifdef __cplusplus
} 
#endif
