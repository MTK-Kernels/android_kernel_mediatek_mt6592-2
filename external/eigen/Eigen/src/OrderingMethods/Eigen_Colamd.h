// Copyright (C) 2012 Desire Nuentsa Wakam <desire.nuentsa_wakam@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

// This file is modified from the colamd/symamd library. The copyright is below

//   Copyright (c) 1998-2003 by the University of Florida.
//   this program, provided that the Copyright, this License, and the

  
#ifndef EIGEN_COLAMD_H
#define EIGEN_COLAMD_H

namespace internal {
#ifndef COLAMD_NDEBUG
#define COLAMD_NDEBUG
#endif 

#define COLAMD_KNOBS 20

#define COLAMD_STATS 20 

#define COLAMD_DENSE_ROW 0

#define COLAMD_DENSE_COL 1

#define COLAMD_DEFRAG_COUNT 2

#define COLAMD_STATUS 3

 
#define COLAMD_INFO1 4
#define COLAMD_INFO2 5
#define COLAMD_INFO3 6

#define COLAMD_OK       (0)
#define COLAMD_OK_BUT_JUMBLED     (1)
#define COLAMD_ERROR_A_not_present    (-1)
#define COLAMD_ERROR_p_not_present    (-2)
#define COLAMD_ERROR_nrow_negative    (-3)
#define COLAMD_ERROR_ncol_negative    (-4)
#define COLAMD_ERROR_nnz_negative   (-5)
#define COLAMD_ERROR_p0_nonzero     (-6)
#define COLAMD_ERROR_A_too_small    (-7)
#define COLAMD_ERROR_col_length_negative  (-8)
#define COLAMD_ERROR_row_index_out_of_bounds  (-9)
#define COLAMD_ERROR_out_of_memory    (-10)
#define COLAMD_ERROR_internal_error   (-999)


#define COLAMD_MAX(a,b) (((a) > (b)) ? (a) : (b))
#define COLAMD_MIN(a,b) (((a) < (b)) ? (a) : (b))

#define ONES_COMPLEMENT(r) (-(r)-1)


#define COLAMD_EMPTY (-1)

#define ALIVE (0)
#define DEAD  (-1)

#define DEAD_PRINCIPAL    (-1)
#define DEAD_NON_PRINCIPAL  (-2)

#define ROW_IS_DEAD(r)      ROW_IS_MARKED_DEAD (Row[r].shared2.mark)
#define ROW_IS_MARKED_DEAD(row_mark)  (row_mark < ALIVE)
#define ROW_IS_ALIVE(r)     (Row [r].shared2.mark >= ALIVE)
#define COL_IS_DEAD(c)      (Col [c].start < ALIVE)
#define COL_IS_ALIVE(c)     (Col [c].start >= ALIVE)
#define COL_IS_DEAD_PRINCIPAL(c)  (Col [c].start == DEAD_PRINCIPAL)
#define KILL_ROW(r)     { Row [r].shared2.mark = DEAD ; }
#define KILL_PRINCIPAL_COL(c)   { Col [c].start = DEAD_PRINCIPAL ; }
#define KILL_NON_PRINCIPAL_COL(c) { Col [c].start = DEAD_NON_PRINCIPAL ; }


template <typename Index>
struct colamd_col
{
  Index start ;   
  
  Index length ;  
  union
  {
    Index thickness ; 
    
    Index parent ;  
    
  } shared1 ;
  union
  {
    Index score ; 
    Index order ; 
  } shared2 ;
  union
  {
    Index headhash ;  
    
    Index hash ;  
    Index prev ;  
    
  } shared3 ;
  union
  {
    Index degree_next ; 
    Index hash_next ;   
  } shared4 ;
  
};
 
template <typename Index>
struct Colamd_Row
{
  Index start ;   
  Index length ;  
  union
  {
    Index degree ;  
    Index p ;   
  } shared1 ;
  union
  {
    Index mark ;  
    Index first_column ;
  } shared2 ;
  
};
 
 
template <typename Index>
inline Index colamd_c(Index n_col) 
{ return Index( ((n_col) + 1) * sizeof (colamd_col<Index>) / sizeof (Index) ) ; }

template <typename Index>
inline Index  colamd_r(Index n_row)
{ return Index(((n_row) + 1) * sizeof (Colamd_Row<Index>) / sizeof (Index)); }

template <typename Index>
static Index init_rows_cols (Index n_row, Index n_col, Colamd_Row<Index> Row [], colamd_col<Index> col [], Index A [], Index p [], Index stats[COLAMD_STATS] ); 

template <typename Index>
static void init_scoring (Index n_row, Index n_col, Colamd_Row<Index> Row [], colamd_col<Index> Col [], Index A [], Index head [], double knobs[COLAMD_KNOBS], Index *p_n_row2, Index *p_n_col2, Index *p_max_deg);

template <typename Index>
static Index find_ordering (Index n_row, Index n_col, Index Alen, Colamd_Row<Index> Row [], colamd_col<Index> Col [], Index A [], Index head [], Index n_col2, Index max_deg, Index pfree);

template <typename Index>
static void order_children (Index n_col, colamd_col<Index> Col [], Index p []);

template <typename Index>
static void detect_super_cols (colamd_col<Index> Col [], Index A [], Index head [], Index row_start, Index row_length ) ;

template <typename Index>
static Index garbage_collection (Index n_row, Index n_col, Colamd_Row<Index> Row [], colamd_col<Index> Col [], Index A [], Index *pfree) ;

template <typename Index>
static inline  Index clear_mark (Index n_row, Colamd_Row<Index> Row [] ) ;


#define COLAMD_DEBUG0(params) ;
#define COLAMD_DEBUG1(params) ;
#define COLAMD_DEBUG2(params) ;
#define COLAMD_DEBUG3(params) ;
#define COLAMD_DEBUG4(params) ;

#define COLAMD_ASSERT(expression) ((void) 0)


template <typename Index>
inline Index colamd_recommended ( Index nnz, Index n_row, Index n_col)
{
  if ((nnz) < 0 || (n_row) < 0 || (n_col) < 0)
    return (-1);
  else
    return (2 * (nnz) + colamd_c (n_col) + colamd_r (n_row) + (n_col) + ((nnz) / 5)); 
}


static inline void colamd_set_defaults(double knobs[COLAMD_KNOBS])
{
  
  
  int i ;

  if (!knobs)
  {
    return ;      
  }
  for (i = 0 ; i < COLAMD_KNOBS ; i++)
  {
    knobs [i] = 0 ;
  }
  knobs [COLAMD_DENSE_ROW] = 0.5 ;  
  knobs [COLAMD_DENSE_COL] = 0.5 ;  
}

template <typename Index>
static bool colamd(Index n_row, Index n_col, Index Alen, Index *A, Index *p, double knobs[COLAMD_KNOBS], Index stats[COLAMD_STATS])
{
  
  
  Index i ;     
  Index nnz ;     
  Index Row_size ;    
  Index Col_size ;    
  Index need ;      
  Colamd_Row<Index> *Row ;   
  colamd_col<Index> *Col ;   
  Index n_col2 ;    
  Index n_row2 ;    
  Index ngarbage ;    
  Index max_deg ;   
  double default_knobs [COLAMD_KNOBS] ; 
  
  
  
  
  if (!stats)
  {
    COLAMD_DEBUG0 (("colamd: stats not present\n")) ;
    return (false) ;
  }
  for (i = 0 ; i < COLAMD_STATS ; i++)
  {
    stats [i] = 0 ;
  }
  stats [COLAMD_STATUS] = COLAMD_OK ;
  stats [COLAMD_INFO1] = -1 ;
  stats [COLAMD_INFO2] = -1 ;
  
  if (!A)   
  {
    stats [COLAMD_STATUS] = COLAMD_ERROR_A_not_present ;
    COLAMD_DEBUG0 (("colamd: A not present\n")) ;
    return (false) ;
  }
  
  if (!p)   
  {
    stats [COLAMD_STATUS] = COLAMD_ERROR_p_not_present ;
    COLAMD_DEBUG0 (("colamd: p not present\n")) ;
    return (false) ;
  }
  
  if (n_row < 0)  
  {
    stats [COLAMD_STATUS] = COLAMD_ERROR_nrow_negative ;
    stats [COLAMD_INFO1] = n_row ;
    COLAMD_DEBUG0 (("colamd: nrow negative %d\n", n_row)) ;
    return (false) ;
  }
  
  if (n_col < 0)  
  {
    stats [COLAMD_STATUS] = COLAMD_ERROR_ncol_negative ;
    stats [COLAMD_INFO1] = n_col ;
    COLAMD_DEBUG0 (("colamd: ncol negative %d\n", n_col)) ;
    return (false) ;
  }
  
  nnz = p [n_col] ;
  if (nnz < 0)  
  {
    stats [COLAMD_STATUS] = COLAMD_ERROR_nnz_negative ;
    stats [COLAMD_INFO1] = nnz ;
    COLAMD_DEBUG0 (("colamd: number of entries negative %d\n", nnz)) ;
    return (false) ;
  }
  
  if (p [0] != 0)
  {
    stats [COLAMD_STATUS] = COLAMD_ERROR_p0_nonzero ;
    stats [COLAMD_INFO1] = p [0] ;
    COLAMD_DEBUG0 (("colamd: p[0] not zero %d\n", p [0])) ;
    return (false) ;
  }
  
  
  
  if (!knobs)
  {
    colamd_set_defaults (default_knobs) ;
    knobs = default_knobs ;
  }
  
  
  
  Col_size = colamd_c (n_col) ;
  Row_size = colamd_r (n_row) ;
  need = 2*nnz + n_col + Col_size + Row_size ;
  
  if (need > Alen)
  {
    
    stats [COLAMD_STATUS] = COLAMD_ERROR_A_too_small ;
    stats [COLAMD_INFO1] = need ;
    stats [COLAMD_INFO2] = Alen ;
    COLAMD_DEBUG0 (("colamd: Need Alen >= %d, given only Alen = %d\n", need,Alen));
    return (false) ;
  }
  
  Alen -= Col_size + Row_size ;
  Col = (colamd_col<Index> *) &A [Alen] ;
  Row = (Colamd_Row<Index> *) &A [Alen + Col_size] ;

  
  
  if (!Eigen::internal::init_rows_cols (n_row, n_col, Row, Col, A, p, stats))
  {
    
    COLAMD_DEBUG0 (("colamd: Matrix invalid\n")) ;
    return (false) ;
  }
  
  

  Eigen::internal::init_scoring (n_row, n_col, Row, Col, A, p, knobs,
		&n_row2, &n_col2, &max_deg) ;
  
  
  
  ngarbage = Eigen::internal::find_ordering (n_row, n_col, Alen, Row, Col, A, p,
			    n_col2, max_deg, 2*nnz) ;
  
  
  
  Eigen::internal::order_children (n_col, Col, p) ;
  
  
  
  stats [COLAMD_DENSE_ROW] = n_row - n_row2 ;
  stats [COLAMD_DENSE_COL] = n_col - n_col2 ;
  stats [COLAMD_DEFRAG_COUNT] = ngarbage ;
  COLAMD_DEBUG0 (("colamd: done.\n")) ; 
  return (true) ;
}





template <typename Index>
static Index init_rows_cols  
  (
    

    Index n_row,      
    Index n_col,      
    Colamd_Row<Index> Row [],    
    colamd_col<Index> Col [],    
    Index A [],     
    Index p [],     
    Index stats [COLAMD_STATS]   
    )
{
  

  Index col ;     
  Index row ;     
  Index *cp ;     
  Index *cp_end ;   
  Index *rp ;     
  Index *rp_end ;   
  Index last_row ;    

  

  for (col = 0 ; col < n_col ; col++)
  {
    Col [col].start = p [col] ;
    Col [col].length = p [col+1] - p [col] ;

    if (Col [col].length < 0)
    {
      
      stats [COLAMD_STATUS] = COLAMD_ERROR_col_length_negative ;
      stats [COLAMD_INFO1] = col ;
      stats [COLAMD_INFO2] = Col [col].length ;
      COLAMD_DEBUG0 (("colamd: col %d length %d < 0\n", col, Col [col].length)) ;
      return (false) ;
    }

    Col [col].shared1.thickness = 1 ;
    Col [col].shared2.score = 0 ;
    Col [col].shared3.prev = COLAMD_EMPTY ;
    Col [col].shared4.degree_next = COLAMD_EMPTY ;
  }

  

  

  stats [COLAMD_INFO3] = 0 ;  

  for (row = 0 ; row < n_row ; row++)
  {
    Row [row].length = 0 ;
    Row [row].shared2.mark = -1 ;
  }

  for (col = 0 ; col < n_col ; col++)
  {
    last_row = -1 ;

    cp = &A [p [col]] ;
    cp_end = &A [p [col+1]] ;

    while (cp < cp_end)
    {
      row = *cp++ ;

      
      if (row < 0 || row >= n_row)
      {
	stats [COLAMD_STATUS] = COLAMD_ERROR_row_index_out_of_bounds ;
	stats [COLAMD_INFO1] = col ;
	stats [COLAMD_INFO2] = row ;
	stats [COLAMD_INFO3] = n_row ;
	COLAMD_DEBUG0 (("colamd: row %d col %d out of bounds\n", row, col)) ;
	return (false) ;
      }

      if (row <= last_row || Row [row].shared2.mark == col)
      {
	
	
	stats [COLAMD_STATUS] = COLAMD_OK_BUT_JUMBLED ;
	stats [COLAMD_INFO1] = col ;
	stats [COLAMD_INFO2] = row ;
	(stats [COLAMD_INFO3]) ++ ;
	COLAMD_DEBUG1 (("colamd: row %d col %d unsorted/duplicate\n",row,col));
      }

      if (Row [row].shared2.mark != col)
      {
	Row [row].length++ ;
      }
      else
      {
	
	
	Col [col].length-- ;
      }

      
      Row [row].shared2.mark = col ;

      last_row = row ;
    }
  }

  

  
  
  Row [0].start = p [n_col] ;
  Row [0].shared1.p = Row [0].start ;
  Row [0].shared2.mark = -1 ;
  for (row = 1 ; row < n_row ; row++)
  {
    Row [row].start = Row [row-1].start + Row [row-1].length ;
    Row [row].shared1.p = Row [row].start ;
    Row [row].shared2.mark = -1 ;
  }

  

  if (stats [COLAMD_STATUS] == COLAMD_OK_BUT_JUMBLED)
  {
    
    for (col = 0 ; col < n_col ; col++)
    {
      cp = &A [p [col]] ;
      cp_end = &A [p [col+1]] ;
      while (cp < cp_end)
      {
	row = *cp++ ;
	if (Row [row].shared2.mark != col)
	{
	  A [(Row [row].shared1.p)++] = col ;
	  Row [row].shared2.mark = col ;
	}
      }
    }
  }
  else
  {
    
    for (col = 0 ; col < n_col ; col++)
    {
      cp = &A [p [col]] ;
      cp_end = &A [p [col+1]] ;
      while (cp < cp_end)
      {
	A [(Row [*cp++].shared1.p)++] = col ;
      }
    }
  }

  

  for (row = 0 ; row < n_row ; row++)
  {
    Row [row].shared2.mark = 0 ;
    Row [row].shared1.degree = Row [row].length ;
  }

  

  if (stats [COLAMD_STATUS] == COLAMD_OK_BUT_JUMBLED)
  {
    COLAMD_DEBUG0 (("colamd: reconstructing column form, matrix jumbled\n")) ;


    

    
    
    
    
    Col [0].start = 0 ;
    p [0] = Col [0].start ;
    for (col = 1 ; col < n_col ; col++)
    {
      
      
      Col [col].start = Col [col-1].start + Col [col-1].length ;
      p [col] = Col [col].start ;
    }

    

    for (row = 0 ; row < n_row ; row++)
    {
      rp = &A [Row [row].start] ;
      rp_end = rp + Row [row].length ;
      while (rp < rp_end)
      {
	A [(p [*rp++])++] = row ;
      }
    }
  }

  

  return (true) ;
}



template <typename Index>
static void init_scoring
  (
    

    Index n_row,      
    Index n_col,      
    Colamd_Row<Index> Row [],    
    colamd_col<Index> Col [],    
    Index A [],     
    Index head [],    
    double knobs [COLAMD_KNOBS],
    Index *p_n_row2,    
    Index *p_n_col2,    
    Index *p_max_deg    
    )
{
  

  Index c ;     
  Index r, row ;    
  Index *cp ;     
  Index deg ;     
  Index *cp_end ;   
  Index *new_cp ;   
  Index col_length ;    
  Index score ;     
  Index n_col2 ;    
  Index n_row2 ;    
  Index dense_row_count ; 
  Index dense_col_count ; 
  Index min_score ;   
  Index max_deg ;   
  Index next_col ;    


  

  dense_row_count = COLAMD_MAX (0, COLAMD_MIN (knobs [COLAMD_DENSE_ROW] * n_col, n_col)) ;
  dense_col_count = COLAMD_MAX (0, COLAMD_MIN (knobs [COLAMD_DENSE_COL] * n_row, n_row)) ;
  COLAMD_DEBUG1 (("colamd: densecount: %d %d\n", dense_row_count, dense_col_count)) ;
  max_deg = 0 ;
  n_col2 = n_col ;
  n_row2 = n_row ;

  

  
  
  for (c = n_col-1 ; c >= 0 ; c--)
  {
    deg = Col [c].length ;
    if (deg == 0)
    {
      
      Col [c].shared2.order = --n_col2 ;
      KILL_PRINCIPAL_COL (c) ;
    }
  }
  COLAMD_DEBUG1 (("colamd: null columns killed: %d\n", n_col - n_col2)) ;

  

  
  for (c = n_col-1 ; c >= 0 ; c--)
  {
    
    if (COL_IS_DEAD (c))
    {
      continue ;
    }
    deg = Col [c].length ;
    if (deg > dense_col_count)
    {
      
      Col [c].shared2.order = --n_col2 ;
      
      cp = &A [Col [c].start] ;
      cp_end = cp + Col [c].length ;
      while (cp < cp_end)
      {
	Row [*cp++].shared1.degree-- ;
      }
      KILL_PRINCIPAL_COL (c) ;
    }
  }
  COLAMD_DEBUG1 (("colamd: Dense and null columns killed: %d\n", n_col - n_col2)) ;

  

  for (r = 0 ; r < n_row ; r++)
  {
    deg = Row [r].shared1.degree ;
    COLAMD_ASSERT (deg >= 0 && deg <= n_col) ;
    if (deg > dense_row_count || deg == 0)
    {
      
      KILL_ROW (r) ;
      --n_row2 ;
    }
    else
    {
      
      max_deg = COLAMD_MAX (max_deg, deg) ;
    }
  }
  COLAMD_DEBUG1 (("colamd: Dense and null rows killed: %d\n", n_row - n_row2)) ;

  

  
  
  
  

  
  for (c = n_col-1 ; c >= 0 ; c--)
  {
    
    if (COL_IS_DEAD (c))
    {
      continue ;
    }
    score = 0 ;
    cp = &A [Col [c].start] ;
    new_cp = cp ;
    cp_end = cp + Col [c].length ;
    while (cp < cp_end)
    {
      
      row = *cp++ ;
      
      if (ROW_IS_DEAD (row))
      {
	continue ;
      }
      
      *new_cp++ = row ;
      
      score += Row [row].shared1.degree - 1 ;
      
      score = COLAMD_MIN (score, n_col) ;
    }
    
    col_length = (Index) (new_cp - &A [Col [c].start]) ;
    if (col_length == 0)
    {
      
      
      COLAMD_DEBUG2 (("Newly null killed: %d\n", c)) ;
      Col [c].shared2.order = --n_col2 ;
      KILL_PRINCIPAL_COL (c) ;
    }
    else
    {
      
      COLAMD_ASSERT (score >= 0) ;
      COLAMD_ASSERT (score <= n_col) ;
      Col [c].length = col_length ;
      Col [c].shared2.score = score ;
    }
  }
  COLAMD_DEBUG1 (("colamd: Dense, null, and newly-null columns killed: %d\n",
		  n_col-n_col2)) ;

  
  
  
  

  


  
  for (c = 0 ; c <= n_col ; c++)
  {
    head [c] = COLAMD_EMPTY ;
  }
  min_score = n_col ;
  
  
  for (c = n_col-1 ; c >= 0 ; c--)
  {
    
    if (COL_IS_ALIVE (c))
    {
      COLAMD_DEBUG4 (("place %d score %d minscore %d ncol %d\n",
		      c, Col [c].shared2.score, min_score, n_col)) ;

      

      score = Col [c].shared2.score ;

      COLAMD_ASSERT (min_score >= 0) ;
      COLAMD_ASSERT (min_score <= n_col) ;
      COLAMD_ASSERT (score >= 0) ;
      COLAMD_ASSERT (score <= n_col) ;
      COLAMD_ASSERT (head [score] >= COLAMD_EMPTY) ;

      
      next_col = head [score] ;
      Col [c].shared3.prev = COLAMD_EMPTY ;
      Col [c].shared4.degree_next = next_col ;

      
      
      if (next_col != COLAMD_EMPTY)
      {
	Col [next_col].shared3.prev = c ;
      }
      head [score] = c ;

      
      min_score = COLAMD_MIN (min_score, score) ;


    }
  }


  

  *p_n_col2 = n_col2 ;
  *p_n_row2 = n_row2 ;
  *p_max_deg = max_deg ;
}



template <typename Index>
static Index find_ordering 
  (
    

    Index n_row,      
    Index n_col,      
    Index Alen,     
    Colamd_Row<Index> Row [],    
    colamd_col<Index> Col [],    
    Index A [],     
    Index head [],    
    Index n_col2,     
    Index max_deg,    
    Index pfree     
    )
{
  

  Index k ;     
  Index pivot_col ;   
  Index *cp ;     
  Index *rp ;     
  Index pivot_row ;   
  Index *new_cp ;   
  Index *new_rp ;   
  Index pivot_row_start ; 
  Index pivot_row_degree ;  
  Index pivot_row_length ;  
  Index pivot_col_score ; 
  Index needed_memory ;   
  Index *cp_end ;   
  Index *rp_end ;   
  Index row ;     
  Index col ;     
  Index max_score ;   
  Index cur_score ;   
  unsigned int hash ;   
  Index head_column ;   
  Index first_col ;   
  Index tag_mark ;    
  Index row_mark ;    
  Index set_difference ;  
  Index min_score ;   
  Index col_thickness ;   
  Index max_mark ;    
  Index pivot_col_thickness ; 
  Index prev_col ;    
  Index next_col ;    
  Index ngarbage ;    


  

  max_mark = INT_MAX - n_col ;  
  tag_mark = Eigen::internal::clear_mark (n_row, Row) ;
  min_score = 0 ;
  ngarbage = 0 ;
  COLAMD_DEBUG1 (("colamd: Ordering, n_col2=%d\n", n_col2)) ;

  

  for (k = 0 ; k < n_col2 ; )
  {

    

    
    COLAMD_ASSERT (min_score >= 0) ;
    COLAMD_ASSERT (min_score <= n_col) ;
    COLAMD_ASSERT (head [min_score] >= COLAMD_EMPTY) ;

    
    while (head [min_score] == COLAMD_EMPTY && min_score < n_col)
    {
      min_score++ ;
    }
    pivot_col = head [min_score] ;
    COLAMD_ASSERT (pivot_col >= 0 && pivot_col <= n_col) ;
    next_col = Col [pivot_col].shared4.degree_next ;
    head [min_score] = next_col ;
    if (next_col != COLAMD_EMPTY)
    {
      Col [next_col].shared3.prev = COLAMD_EMPTY ;
    }

    COLAMD_ASSERT (COL_IS_ALIVE (pivot_col)) ;
    COLAMD_DEBUG3 (("Pivot col: %d\n", pivot_col)) ;

    
    pivot_col_score = Col [pivot_col].shared2.score ;

    
    Col [pivot_col].shared2.order = k ;

    
    pivot_col_thickness = Col [pivot_col].shared1.thickness ;
    k += pivot_col_thickness ;
    COLAMD_ASSERT (pivot_col_thickness > 0) ;

    

    needed_memory = COLAMD_MIN (pivot_col_score, n_col - k) ;
    if (pfree + needed_memory >= Alen)
    {
      pfree = Eigen::internal::garbage_collection (n_row, n_col, Row, Col, A, &A [pfree]) ;
      ngarbage++ ;
      
      COLAMD_ASSERT (pfree + needed_memory < Alen) ;
      
      tag_mark = Eigen::internal::clear_mark (n_row, Row) ;

    }

    

    
    pivot_row_start = pfree ;

    
    pivot_row_degree = 0 ;

    
    
    Col [pivot_col].shared1.thickness = -pivot_col_thickness ;

    
    cp = &A [Col [pivot_col].start] ;
    cp_end = cp + Col [pivot_col].length ;
    while (cp < cp_end)
    {
      
      row = *cp++ ;
      COLAMD_DEBUG4 (("Pivot col pattern %d %d\n", ROW_IS_ALIVE (row), row)) ;
      
      if (ROW_IS_DEAD (row))
      {
	continue ;
      }
      rp = &A [Row [row].start] ;
      rp_end = rp + Row [row].length ;
      while (rp < rp_end)
      {
	
	col = *rp++ ;
	
	col_thickness = Col [col].shared1.thickness ;
	if (col_thickness > 0 && COL_IS_ALIVE (col))
	{
	  
	  Col [col].shared1.thickness = -col_thickness ;
	  COLAMD_ASSERT (pfree < Alen) ;
	  
	  A [pfree++] = col ;
	  pivot_row_degree += col_thickness ;
	}
      }
    }

    
    Col [pivot_col].shared1.thickness = pivot_col_thickness ;
    max_deg = COLAMD_MAX (max_deg, pivot_row_degree) ;


    

    
    cp = &A [Col [pivot_col].start] ;
    cp_end = cp + Col [pivot_col].length ;
    while (cp < cp_end)
    {
      
      row = *cp++ ;
      COLAMD_DEBUG3 (("Kill row in pivot col: %d\n", row)) ;
      KILL_ROW (row) ;
    }

    

    pivot_row_length = pfree - pivot_row_start ;
    if (pivot_row_length > 0)
    {
      
      pivot_row = A [Col [pivot_col].start] ;
      COLAMD_DEBUG3 (("Pivotal row is %d\n", pivot_row)) ;
    }
    else
    {
      
      pivot_row = COLAMD_EMPTY ;
      COLAMD_ASSERT (pivot_row_length == 0) ;
    }
    COLAMD_ASSERT (Col [pivot_col].length > 0 || pivot_row_length == 0) ;

    

    
    
    
    
    
    

    
    
    
    
    
    
    
    
    

    

    COLAMD_DEBUG3 (("** Computing set differences phase. **\n")) ;

    

    COLAMD_DEBUG3 (("Pivot row: ")) ;
    
    rp = &A [pivot_row_start] ;
    rp_end = rp + pivot_row_length ;
    while (rp < rp_end)
    {
      col = *rp++ ;
      COLAMD_ASSERT (COL_IS_ALIVE (col) && col != pivot_col) ;
      COLAMD_DEBUG3 (("Col: %d\n", col)) ;

      
      col_thickness = -Col [col].shared1.thickness ;
      COLAMD_ASSERT (col_thickness > 0) ;
      Col [col].shared1.thickness = col_thickness ;

      

      cur_score = Col [col].shared2.score ;
      prev_col = Col [col].shared3.prev ;
      next_col = Col [col].shared4.degree_next ;
      COLAMD_ASSERT (cur_score >= 0) ;
      COLAMD_ASSERT (cur_score <= n_col) ;
      COLAMD_ASSERT (cur_score >= COLAMD_EMPTY) ;
      if (prev_col == COLAMD_EMPTY)
      {
	head [cur_score] = next_col ;
      }
      else
      {
	Col [prev_col].shared4.degree_next = next_col ;
      }
      if (next_col != COLAMD_EMPTY)
      {
	Col [next_col].shared3.prev = prev_col ;
      }

      

      cp = &A [Col [col].start] ;
      cp_end = cp + Col [col].length ;
      while (cp < cp_end)
      {
	
	row = *cp++ ;
	row_mark = Row [row].shared2.mark ;
	
	if (ROW_IS_MARKED_DEAD (row_mark))
	{
	  continue ;
	}
	COLAMD_ASSERT (row != pivot_row) ;
	set_difference = row_mark - tag_mark ;
	
	if (set_difference < 0)
	{
	  COLAMD_ASSERT (Row [row].shared1.degree <= max_deg) ;
	  set_difference = Row [row].shared1.degree ;
	}
	
	set_difference -= col_thickness ;
	COLAMD_ASSERT (set_difference >= 0) ;
	
	if (set_difference == 0)
	{
	  COLAMD_DEBUG3 (("aggressive absorption. Row: %d\n", row)) ;
	  KILL_ROW (row) ;
	}
	else
	{
	  
	  Row [row].shared2.mark = set_difference + tag_mark ;
	}
      }
    }


    

    COLAMD_DEBUG3 (("** Adding set differences phase. **\n")) ;

    
    rp = &A [pivot_row_start] ;
    rp_end = rp + pivot_row_length ;
    while (rp < rp_end)
    {
      
      col = *rp++ ;
      COLAMD_ASSERT (COL_IS_ALIVE (col) && col != pivot_col) ;
      hash = 0 ;
      cur_score = 0 ;
      cp = &A [Col [col].start] ;
      
      new_cp = cp ;
      cp_end = cp + Col [col].length ;

      COLAMD_DEBUG4 (("Adding set diffs for Col: %d.\n", col)) ;

      while (cp < cp_end)
      {
	
	row = *cp++ ;
	COLAMD_ASSERT(row >= 0 && row < n_row) ;
	row_mark = Row [row].shared2.mark ;
	
	if (ROW_IS_MARKED_DEAD (row_mark))
	{
	  continue ;
	}
	COLAMD_ASSERT (row_mark > tag_mark) ;
	
	*new_cp++ = row ;
	
	hash += row ;
	
	cur_score += row_mark - tag_mark ;
	
	cur_score = COLAMD_MIN (cur_score, n_col) ;
      }

      
      Col [col].length = (Index) (new_cp - &A [Col [col].start]) ;

      

      if (Col [col].length == 0)
      {
	COLAMD_DEBUG4 (("further mass elimination. Col: %d\n", col)) ;
	
	KILL_PRINCIPAL_COL (col) ;
	pivot_row_degree -= Col [col].shared1.thickness ;
	COLAMD_ASSERT (pivot_row_degree >= 0) ;
	
	Col [col].shared2.order = k ;
	
	k += Col [col].shared1.thickness ;
      }
      else
      {
	

	COLAMD_DEBUG4 (("Preparing supercol detection for Col: %d.\n", col)) ;

	
	Col [col].shared2.score = cur_score ;

	
	hash %= n_col + 1 ;

	COLAMD_DEBUG4 ((" Hash = %d, n_col = %d.\n", hash, n_col)) ;
	COLAMD_ASSERT (hash <= n_col) ;

	head_column = head [hash] ;
	if (head_column > COLAMD_EMPTY)
	{
	  
	  
	  first_col = Col [head_column].shared3.headhash ;
	  Col [head_column].shared3.headhash = col ;
	}
	else
	{
	  
	  first_col = - (head_column + 2) ;
	  head [hash] = - (col + 2) ;
	}
	Col [col].shared4.hash_next = first_col ;

	
	Col [col].shared3.hash = (Index) hash ;
	COLAMD_ASSERT (COL_IS_ALIVE (col)) ;
      }
    }

    

    

    COLAMD_DEBUG3 (("** Supercolumn detection phase. **\n")) ;

    Eigen::internal::detect_super_cols (Col, A, head, pivot_row_start, pivot_row_length) ;

    

    KILL_PRINCIPAL_COL (pivot_col) ;

    

    tag_mark += (max_deg + 1) ;
    if (tag_mark >= max_mark)
    {
      COLAMD_DEBUG2 (("clearing tag_mark\n")) ;
      tag_mark = Eigen::internal::clear_mark (n_row, Row) ;
    }

    

    COLAMD_DEBUG3 (("** Finalize scores phase. **\n")) ;

    
    rp = &A [pivot_row_start] ;
    
    new_rp = rp ;
    rp_end = rp + pivot_row_length ;
    while (rp < rp_end)
    {
      col = *rp++ ;
      
      if (COL_IS_DEAD (col))
      {
	continue ;
      }
      *new_rp++ = col ;
      
      A [Col [col].start + (Col [col].length++)] = pivot_row ;

      
      
      
      cur_score = Col [col].shared2.score + pivot_row_degree ;

      
      
      
      max_score = n_col - k - Col [col].shared1.thickness ;

      
      cur_score -= Col [col].shared1.thickness ;

      
      cur_score = COLAMD_MIN (cur_score, max_score) ;
      COLAMD_ASSERT (cur_score >= 0) ;

      
      Col [col].shared2.score = cur_score ;

      

      COLAMD_ASSERT (min_score >= 0) ;
      COLAMD_ASSERT (min_score <= n_col) ;
      COLAMD_ASSERT (cur_score >= 0) ;
      COLAMD_ASSERT (cur_score <= n_col) ;
      COLAMD_ASSERT (head [cur_score] >= COLAMD_EMPTY) ;
      next_col = head [cur_score] ;
      Col [col].shared4.degree_next = next_col ;
      Col [col].shared3.prev = COLAMD_EMPTY ;
      if (next_col != COLAMD_EMPTY)
      {
	Col [next_col].shared3.prev = col ;
      }
      head [cur_score] = col ;

      
      min_score = COLAMD_MIN (min_score, cur_score) ;

    }

    

    if (pivot_row_degree > 0)
    {
      
      
      Row [pivot_row].start  = pivot_row_start ;
      Row [pivot_row].length = (Index) (new_rp - &A[pivot_row_start]) ;
      Row [pivot_row].shared1.degree = pivot_row_degree ;
      Row [pivot_row].shared2.mark = 0 ;
      
    }
  }

  

  return (ngarbage) ;
}



template <typename Index>
static inline  void order_children
(
  

  Index n_col,      
  colamd_col<Index> Col [],    
  Index p []      
  )
{
  

  Index i ;     
  Index c ;     
  Index parent ;    
  Index order ;     

  

  for (i = 0 ; i < n_col ; i++)
  {
    
    COLAMD_ASSERT (COL_IS_DEAD (i)) ;
    if (!COL_IS_DEAD_PRINCIPAL (i) && Col [i].shared2.order == COLAMD_EMPTY)
    {
      parent = i ;
      
      do
      {
	parent = Col [parent].shared1.parent ;
      } while (!COL_IS_DEAD_PRINCIPAL (parent)) ;

      
      
      c = i ;
      
      order = Col [parent].shared2.order ;

      do
      {
	COLAMD_ASSERT (Col [c].shared2.order == COLAMD_EMPTY) ;

	
	Col [c].shared2.order = order++ ;
	
	Col [c].shared1.parent = parent ;

	
	c = Col [c].shared1.parent ;

	
	
	
      } while (Col [c].shared2.order == COLAMD_EMPTY) ;

      
      Col [parent].shared2.order = order ;
    }
  }

  

  for (c = 0 ; c < n_col ; c++)
  {
    p [Col [c].shared2.order] = c ;
  }
}



template <typename Index>
static void detect_super_cols
(
  
  
  colamd_col<Index> Col [],    
  Index A [],     
  Index head [],    
  Index row_start,    
  Index row_length    
)
{
  

  Index hash ;      
  Index *rp ;     
  Index c ;     
  Index super_c ;   
  Index *cp1 ;      
  Index *cp2 ;      
  Index length ;    
  Index prev_c ;    
  Index i ;     
  Index *rp_end ;   
  Index col ;     
  Index head_column ;   
  Index first_col ;   

  

  rp = &A [row_start] ;
  rp_end = rp + row_length ;
  while (rp < rp_end)
  {
    col = *rp++ ;
    if (COL_IS_DEAD (col))
    {
      continue ;
    }

    
    hash = Col [col].shared3.hash ;
    COLAMD_ASSERT (hash <= n_col) ;

    

    head_column = head [hash] ;
    if (head_column > COLAMD_EMPTY)
    {
      first_col = Col [head_column].shared3.headhash ;
    }
    else
    {
      first_col = - (head_column + 2) ;
    }

    

    for (super_c = first_col ; super_c != COLAMD_EMPTY ;
	 super_c = Col [super_c].shared4.hash_next)
    {
      COLAMD_ASSERT (COL_IS_ALIVE (super_c)) ;
      COLAMD_ASSERT (Col [super_c].shared3.hash == hash) ;
      length = Col [super_c].length ;

      
      prev_c = super_c ;

      

      for (c = Col [super_c].shared4.hash_next ;
	   c != COLAMD_EMPTY ; c = Col [c].shared4.hash_next)
      {
	COLAMD_ASSERT (c != super_c) ;
	COLAMD_ASSERT (COL_IS_ALIVE (c)) ;
	COLAMD_ASSERT (Col [c].shared3.hash == hash) ;

	
	if (Col [c].length != length ||
	    Col [c].shared2.score != Col [super_c].shared2.score)
	{
	  prev_c = c ;
	  continue ;
	}

	
	cp1 = &A [Col [super_c].start] ;
	cp2 = &A [Col [c].start] ;

	for (i = 0 ; i < length ; i++)
	{
	  
	  COLAMD_ASSERT (ROW_IS_ALIVE (*cp1))  ;
	  COLAMD_ASSERT (ROW_IS_ALIVE (*cp2))  ;
	  
	  
	  if (*cp1++ != *cp2++)
	  {
	    break ;
	  }
	}

	
	if (i != length)
	{
	  prev_c = c ;
	  continue ;
	}

	

	COLAMD_ASSERT (Col [c].shared2.score == Col [super_c].shared2.score) ;

	Col [super_c].shared1.thickness += Col [c].shared1.thickness ;
	Col [c].shared1.parent = super_c ;
	KILL_NON_PRINCIPAL_COL (c) ;
	
	Col [c].shared2.order = COLAMD_EMPTY ;
	
	Col [prev_c].shared4.hash_next = Col [c].shared4.hash_next ;
      }
    }

    

    if (head_column > COLAMD_EMPTY)
    {
      
      Col [head_column].shared3.headhash = COLAMD_EMPTY ;
    }
    else
    {
      
      head [hash] = COLAMD_EMPTY ;
    }
  }
}



template <typename Index>
static Index garbage_collection  
  (
    
    
    Index n_row,      
    Index n_col,      
    Colamd_Row<Index> Row [],    
    colamd_col<Index> Col [],    
    Index A [],     
    Index *pfree      
    )
{
  

  Index *psrc ;     
  Index *pdest ;    
  Index j ;     
  Index r ;     
  Index c ;     
  Index length ;    

  

  pdest = &A[0] ;
  for (c = 0 ; c < n_col ; c++)
  {
    if (COL_IS_ALIVE (c))
    {
      psrc = &A [Col [c].start] ;

      
      COLAMD_ASSERT (pdest <= psrc) ;
      Col [c].start = (Index) (pdest - &A [0]) ;
      length = Col [c].length ;
      for (j = 0 ; j < length ; j++)
      {
	r = *psrc++ ;
	if (ROW_IS_ALIVE (r))
	{
	  *pdest++ = r ;
	}
      }
      Col [c].length = (Index) (pdest - &A [Col [c].start]) ;
    }
  }

  

  for (r = 0 ; r < n_row ; r++)
  {
    if (ROW_IS_ALIVE (r))
    {
      if (Row [r].length == 0)
      {
	
	COLAMD_DEBUG3 (("Defrag row kill\n")) ;
	KILL_ROW (r) ;
      }
      else
      {
	
	psrc = &A [Row [r].start] ;
	Row [r].shared2.first_column = *psrc ;
	COLAMD_ASSERT (ROW_IS_ALIVE (r)) ;
	
	*psrc = ONES_COMPLEMENT (r) ;

      }
    }
  }

  

  psrc = pdest ;
  while (psrc < pfree)
  {
    
    if (*psrc++ < 0)
    {
      psrc-- ;
      
      r = ONES_COMPLEMENT (*psrc) ;
      COLAMD_ASSERT (r >= 0 && r < n_row) ;
      
      *psrc = Row [r].shared2.first_column ;
      COLAMD_ASSERT (ROW_IS_ALIVE (r)) ;

      
      COLAMD_ASSERT (pdest <= psrc) ;
      Row [r].start = (Index) (pdest - &A [0]) ;
      length = Row [r].length ;
      for (j = 0 ; j < length ; j++)
      {
	c = *psrc++ ;
	if (COL_IS_ALIVE (c))
	{
	  *pdest++ = c ;
	}
      }
      Row [r].length = (Index) (pdest - &A [Row [r].start]) ;

    }
  }
  
  COLAMD_ASSERT (debug_rows == 0) ;

  

  return ((Index) (pdest - &A [0])) ;
}



template <typename Index>
static inline  Index clear_mark  
  (
      

    Index n_row,    
    Colamd_Row<Index> Row [] 
    )
{
  

  Index r ;

  for (r = 0 ; r < n_row ; r++)
  {
    if (ROW_IS_ALIVE (r))
    {
      Row [r].shared2.mark = 0 ;
    }
  }
  return (1) ;
}


} 
#endif
