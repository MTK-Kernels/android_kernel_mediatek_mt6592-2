

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2004-2013 OpenWorks LLP
      info@open-works.net

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   The GNU General Public License is contained in the file COPYING.

   Neither the names of the U.S. Department of Energy nor the
   University of California nor the names of its contributors may be
   used to endorse or promote products derived from this software
   without prior written permission.
*/


#include "main_util.h"
#include "ir_match.h"



static 
void setBindee ( MatchInfo* mi, Int n, IRExpr* bindee )
{
   if (n < 0 || n >= N_IRMATCH_BINDERS)
      vpanic("setBindee: out of range index");
   if (mi->bindee[n] != NULL)
      vpanic("setBindee: bindee already set");
   mi->bindee[n] = bindee;
}



static 
Bool matchWrk ( MatchInfo* mi, IRExpr* p, IRExpr* e )
{
   switch (p->tag) {
      case Iex_Binder: 
         setBindee(mi, p->Iex.Binder.binder, e);
         return True;
      case Iex_Unop:
         if (e->tag != Iex_Unop) return False;
         if (p->Iex.Unop.op != e->Iex.Unop.op) return False;
         if (!matchWrk(mi, p->Iex.Unop.arg, e->Iex.Unop.arg))
            return False;
         return True;
      case Iex_Binop:
         if (e->tag != Iex_Binop) return False;
         if (p->Iex.Binop.op != e->Iex.Binop.op) return False;
         if (!matchWrk(mi, p->Iex.Binop.arg1, e->Iex.Binop.arg1))
            return False;
         if (!matchWrk(mi, p->Iex.Binop.arg2, e->Iex.Binop.arg2))
            return False;
         return True;
      case Iex_Load:
         if (e->tag != Iex_Load) return False;
         if (p->Iex.Load.end != e->Iex.Load.end) return False;
         if (p->Iex.Load.ty != e->Iex.Load.ty) return False;
         if (!matchWrk(mi, p->Iex.Load.addr, e->Iex.Load.addr))
            return False;
         return True;
      case Iex_Const:
         if (e->tag != Iex_Const) return False;
         return eqIRConst(p->Iex.Const.con, e->Iex.Const.con);
      default: 
         ppIRExpr(p);
         vpanic("match");
   }
}



Bool matchIRExpr ( MatchInfo* mi, IRExpr* p, IRExpr* e )
{
   Int i;
   for (i = 0; i < N_IRMATCH_BINDERS; i++)
      mi->bindee[i] = NULL;
   return matchWrk(mi, p, e);
}



