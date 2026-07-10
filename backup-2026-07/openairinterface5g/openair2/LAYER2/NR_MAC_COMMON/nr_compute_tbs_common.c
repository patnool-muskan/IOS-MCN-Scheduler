/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/* file: nr_compute_tbs.c
   purpose: Compute NR TBS
   author: Hongzhi WANG (TCL)
*/
#define INDEX_MAX_TBS_TABLE (93)

#include "NR_MAC_gNB/mac_proto.h"
#include "common/utils/nr/nr_common.h"
#include <math.h>
#include "common/utils/hashtable/hashtable.h"

//Table 5.1.2.2-2
static const uint16_t Tbstable_nr[INDEX_MAX_TBS_TABLE] = {
    24,   32,   40,   48,   56,   64,   72,   80,   88,   96,   104,  112,  120,  128,  136,  144,  152,  160,  168,
    176,  184,  192,  208,  224,  240,  256,  272,  288,  304,  320,  336,  352,  368,  384,  408,  432,  456,  480,
    504,  528,  552,  576,  608,  640,  672,  704,  736,  768,  808,  848,  888,  928,  984,  1032, 1064, 1128, 1160,
    1192, 1224, 1256, 1288, 1320, 1352, 1416, 1480, 1544, 1608, 1672, 1736, 1800, 1864, 1928, 2024, 2088, 2152, 2216,
    2280, 2408, 2472, 2536, 2600, 2664, 2728, 2792, 2856, 2976, 3104, 3240, 3368, 3496, 3624, 3752, 3824};

static const uint16_t NPRB_LBRM[7] = {32, 66, 107, 135, 162, 217, 273};



// Transport block size determination according to 6.1.4.2 of TS 38.214
// returns the TBS in bits
uint32_t nr_compute_tbs_new(uint16_t Qm,
                        uint16_t R,
                        uint16_t nbre,
                        uint8_t tb_scaling,
                        uint8_t Nl)
{
  LOG_D(NR_MAC, "In %s: nbre %d, tb_scaling %d Nl %d\n", __FUNCTION__,nbre, tb_scaling, Nl);

  const uint32_t nb_re = nbre;
  // Intermediate number of information bits
  // Rx1024 is tabulated as 10 times the actual code rate
  const uint32_t R_5 = R / 5; // R can be fractional so we can't divide by 10
  // So we ned to right shift by 11 (10 for x1024 and 1 additional as above)
  const uint32_t Ninfo = ((nb_re * R_5 * Qm * Nl) >> 11) >> tb_scaling;
  uint32_t nr_tbs = 0;
  uint32_t Np_info, C, n;
  if (Ninfo <= NR_MAX_PDSCH_TBS) {
    n = max(3, floor(log2(Ninfo)) - 6);
      Np_info = max(24, (Ninfo>>n)<<n);
      for (int i=0; i<INDEX_MAX_TBS_TABLE; i++) {
        if (Tbstable_nr[i] >= Np_info){
          nr_tbs = Tbstable_nr[i];
          break;
        }
      }
  } else {
    n = log2(Ninfo-24)-5;
    Np_info = max(3840, (ROUNDIDIV((Ninfo-24),(1<<n)))<<n);

    if (R <= 2560) {
        C = CEILIDIV((Np_info+24),3816);
        nr_tbs = (C<<3)*CEILIDIV((Np_info+24),(C<<3)) - 24;
    } else {
      if (Np_info > 8424){
          C = CEILIDIV((Np_info+24),8424);
          nr_tbs = (C<<3)*CEILIDIV((Np_info+24),(C<<3)) - 24;
      } else {
        nr_tbs = ((CEILIDIV((Np_info+24),8))<<3) - 24;
      }
    }
  }

  LOG_D(NR_MAC, "In %s: Ninfo %u nb_re %d Qm %d, R %d, tbs %d bits\n", __FUNCTION__, Ninfo, nb_re, Qm, R, nr_tbs);
  return nr_tbs;
}

#ifdef TBS_HASH
static hash_table_t *tbs_hash;
static pthread_mutex_t tbs_mutex = PTHREAD_MUTEX_INITIALIZER;
typedef struct {
    uint32_t tbs;
    uint32_t usage;
  } tbs_cache_t;
#define TBS_HASH_SZ (4*PAGE_SIZE / sizeof(tbs_cache_t))
// static __thread int calls = 0;
#define TBS_KEY(Qm,R,nbpe,tb_scaling,Nl) \
((hash_key_t) Qm                      | \
 (((hash_key_t)(R))           << 16)  | \
 (((hash_key_t)(nbre))        << 24 ) | \
 (((hash_key_t)(tb_scaling))  << 40 ) | \
 (((hash_key_t)(Nl))  << 44 )) 

void tbs_init_hash(void)
{
  pthread_mutex_lock(&tbs_mutex);
  DevAssert(tbs_hash == NULL);
  tbs_hash = hashtable_create(TBS_HASH_SZ , NULL,free);
  DevAssert(tbs_hash != NULL);
  pthread_mutex_unlock(&tbs_mutex);
}

void tbs_destroy_hash(void)
{
  pthread_mutex_lock(&tbs_mutex);
  DevAssert(tbs_hash != NULL);
  hashtable_destroy(&tbs_hash);
  pthread_mutex_unlock(&tbs_mutex);
}

uint32_t nr_compute_tbs_hash(uint16_t Qm,uint16_t R,uint16_t nbre,uint8_t scaling,uint8_t nrOfLayers)
{
  hash_key_t key = HASHTABLE_NOT_A_KEY_VALUE;
  hashtable_rc_t     h_rc;
  pthread_mutex_lock(&tbs_mutex);
  DevAssert(tbs_hash != NULL);
  key = TBS_KEY(Qm, R, nbre, scaling&0x0f, nrOfLayers&0x0f);
  void *data = NULL;
  h_rc = hashtable_get(tbs_hash, key, &data);
  if (h_rc == HASH_TABLE_OK) {
  tbs_cache_t odata = *(tbs_cache_t *)data;
  pthread_mutex_unlock(&tbs_mutex);
  LOG_D(NR_MAC, "In %s:  tbs %d bits\n", __FUNCTION__, odata.tbs);
  return odata.tbs;
  } else {
  tbs_cache_t *idata = malloc(sizeof(*idata));
  idata->tbs = nr_compute_tbs_new(Qm, R, nbre, scaling, nrOfLayers);
  idata->usage = 1;
  h_rc = hashtable_insert(tbs_hash, key, idata);
  pthread_mutex_unlock(&tbs_mutex);
  return idata->tbs;
  }
}

bool tbs_remove(uint32_t ninfo)
{
  pthread_mutex_lock(&tbs_mutex);
  DevAssert(tbs_hash != NULL);
  uint64_t key = ninfo;
  hashtable_rc_t ret = hashtable_remove(tbs_hash, key);
  pthread_mutex_unlock(&tbs_mutex);
  return ret == HASH_TABLE_OK;
}
#endif

uint32_t nr_compute_tbs(uint16_t Qm,
                        uint16_t R,
                        uint16_t nb_rb,
                        uint16_t nb_symb_sch,
                        uint16_t nb_dmrs_prb,
                        uint16_t nb_rb_oh,
                        uint8_t tb_scaling,
                        uint8_t Nl)
{
  uint16_t nbre = nb_rb * min(156, (NR_NB_SC_PER_RB * nb_symb_sch - nb_dmrs_prb - nb_rb_oh));
  uint32_t nr_tbs=0;
#ifdef TBS_HASH
  nr_tbs = nr_compute_tbs_hash(Qm,R,nbre,tb_scaling,Nl);
#else
  nr_tbs = nr_compute_tbs_new(Qm,R,nbre,tb_scaling,Nl);
#endif
return nr_tbs;
}

//tbslbrm calculation according to 5.4.2.1 of 38.212
uint32_t nr_compute_tbslbrm(uint16_t table, uint16_t nb_rb, uint8_t Nl)
{
  uint16_t nb_rb_lbrm = 0;
  for (int i = 0; i < 7; i++) {
    if (NPRB_LBRM[i] >= nb_rb){
    nb_rb_lbrm = NPRB_LBRM[i];
      break;
    }
  }

  int Qm = (table == 1) ? 8 : 6;
  uint32_t R = 948;
  uint32_t nb_re = 156 * nb_rb_lbrm;

  // Intermediate number of information bits
  uint32_t Ninfo = (nb_re * R * Qm * Nl) >> 10;
  uint32_t nr_tbs = 0;
  uint32_t Np_info, n;
  if (Ninfo <= NR_MAX_PDSCH_TBS) {
    n = max(3, floor(log2(Ninfo)) - 6);
    Np_info = max(24, (Ninfo >> n) << n);
    for (int i = 0; i < INDEX_MAX_TBS_TABLE; i++) {
      if (Tbstable_nr[i] >= Np_info){
        nr_tbs = Tbstable_nr[i];
        break;
      }
    }
  } else {
    n = log2(Ninfo - 24) - 5;
    Np_info = max(3840, (ROUNDIDIV((Ninfo - 24), (1 << n))) << n);
    int C;
    if (R <= 256) {
      C = CEILIDIV((Np_info + 24), 3816);
      nr_tbs = (C << 3) * CEILIDIV((Np_info + 24), (C << 3)) - 24;
    }
    else {
      if (Np_info > 8424){
        C = CEILIDIV((Np_info + 24), 8424);
        nr_tbs = (C << 3) * CEILIDIV((Np_info + 24), (C << 3)) - 24;
      }
      else
        nr_tbs = ((CEILIDIV((Np_info + 24), 8)) << 3) - 24;
    }
  }
  return nr_tbs;
}
