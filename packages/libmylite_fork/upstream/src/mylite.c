/*
** MyLite extensions to the private SQLite fork.
*/
#include "sqliteInt.h"

#include <mylite_fork/mylite_sqlite_fork.h>

static int myliteSetColumnType(
  sqlite3 *db,
  const char *zSchema,
  const char *zTable,
  const char *zColumn,
  const MyliteColumnType *pType
);
static int myliteSetEnumColumnType(
  sqlite3 *db,
  const char *zSchema,
  const char *zTable,
  const char *zColumn,
  const struct mylite_sqlite_fork_enum_column_type *pType
);
static int myliteSetSetColumnType(
  sqlite3 *db,
  const char *zSchema,
  const char *zTable,
  const char *zColumn,
  const struct mylite_sqlite_fork_set_column_type *pType
);
static int myliteMakeColumnType(
  const struct mylite_sqlite_fork_column_type *pType,
  MyliteColumnType *pOut
);
static int myliteMakeEnumColumnType(
  sqlite3 *db,
  const struct mylite_sqlite_fork_enum_column_type *pType,
  MyliteColumnType *pOut
);
static int myliteMakeSetColumnType(
  sqlite3 *db,
  const struct mylite_sqlite_fork_set_column_type *pType,
  MyliteColumnType *pOut
);
static int myliteMakeValueListColumnType(
  sqlite3 *db,
  const struct mylite_sqlite_fork_enum_value *aValue,
  sqlite3_uint64 nValue,
  sqlite3_uint64 nMaxValue,
  u8 eType,
  int bRejectComma,
  MyliteColumnType *pOut
);
static int myliteCopyColumnType(
  sqlite3 *db,
  MyliteColumnType *pOut,
  const MyliteColumnType *pIn
);
static void myliteClearColumnTypePayload(sqlite3 *db, MyliteColumnType *pType);
static const char *myliteSchemaName(const char *zSchema);
static void myliteRefreshTableTypeFlag(Table *pTab);

int mylite_sqlite_fork_set_column_type(
  sqlite3 *db,
  const char *zSchema,
  const char *zTable,
  const char *zColumn,
  const struct mylite_sqlite_fork_column_type *pType
){
  MyliteColumnType sqliteType = {0};
  int rc;

  if( db==0 || pType==0 ) return SQLITE_MISUSE;
  rc = myliteMakeColumnType(pType, &sqliteType);
  if( rc!=SQLITE_OK ) return rc;
  return myliteSetColumnType(db, zSchema, zTable, zColumn, &sqliteType);
}

int mylite_sqlite_fork_clear_column_type(
  sqlite3 *db,
  const char *zSchema,
  const char *zTable,
  const char *zColumn
){
  const MyliteColumnType sqliteType = {0};

  if( db==0 ) return SQLITE_MISUSE;
  return myliteSetColumnType(db, zSchema, zTable, zColumn, &sqliteType);
}

int mylite_sqlite_fork_set_enum_column_type(
  sqlite3 *db,
  const char *zSchema,
  const char *zTable,
  const char *zColumn,
  const struct mylite_sqlite_fork_enum_column_type *pType
){
  if( db==0 || pType==0 ) return SQLITE_MISUSE;
  return myliteSetEnumColumnType(db, zSchema, zTable, zColumn, pType);
}

int mylite_sqlite_fork_set_set_column_type(
  sqlite3 *db,
  const char *zSchema,
  const char *zTable,
  const char *zColumn,
  const struct mylite_sqlite_fork_set_column_type *pType
){
  if( db==0 || pType==0 ) return SQLITE_MISUSE;
  return myliteSetSetColumnType(db, zSchema, zTable, zColumn, pType);
}

int mylite_sqlite_fork_last_condition(
  sqlite3 *db,
  struct mylite_sqlite_fork_condition *pOut
){
  if( db==0 || pOut==0 ) return SQLITE_MISUSE;

  sqlite3_mutex_enter(db->mutex);
  pOut->level =
      (enum mylite_sqlite_fork_condition_level)db->myliteCondition.eLevel;
  pOut->mysql_errno = db->myliteCondition.iMyErrno;
  memcpy(pOut->sqlstate, db->myliteCondition.zSqlState,
         sizeof(pOut->sqlstate));
  sqlite3_mutex_leave(db->mutex);
  return SQLITE_OK;
}

int mylite_sqlite_fork_clear_condition(sqlite3 *db){
  if( db==0 ) return SQLITE_MISUSE;
  sqlite3_mutex_enter(db->mutex);
  sqlite3MyliteClearCondition(db);
  sqlite3_mutex_leave(db->mutex);
  return SQLITE_OK;
}

int mylite_sqlite_fork_set_condition(
  sqlite3 *db,
  enum mylite_sqlite_fork_condition_level eLevel,
  unsigned int iMyErrno,
  const char *zSqlState
){
  if( db==0 ) return SQLITE_MISUSE;
  if( eLevel<MYLITE_SQLITE_FORK_CONDITION_NONE
   || eLevel>MYLITE_SQLITE_FORK_CONDITION_WARNING
  ){
    return SQLITE_MISUSE;
  }
  sqlite3_mutex_enter(db->mutex);
  sqlite3MyliteSetCondition(db, (u8)eLevel, iMyErrno, zSqlState);
  sqlite3_mutex_leave(db->mutex);
  return SQLITE_OK;
}

void sqlite3MyliteSetCondition(
  sqlite3 *db,
  u8 eLevel,
  u32 iMyErrno,
  const char *zSqlState
){
  static const char zDefaultSqlState[] = "HY000";
  int i;

  if( db==0 ) return;
  db->myliteCondition.eLevel = eLevel;
  db->myliteCondition.iMyErrno = iMyErrno;
  if( zSqlState==0 || zSqlState[0]==0 ) zSqlState = zDefaultSqlState;
  for(i=0; i<5 && zSqlState[i]!=0; i++){
    db->myliteCondition.zSqlState[i] = zSqlState[i];
  }
  while( i<5 ){
    db->myliteCondition.zSqlState[i++] = '0';
  }
  db->myliteCondition.zSqlState[5] = 0;
}

void sqlite3MyliteSetConstraintCondition(sqlite3 *db, int rc, u8 eConstraint){
  const char *zSqlState = "23000";
  u32 iMyErrno = 0;

  if( (rc & 0xff)!=SQLITE_CONSTRAINT ) return;
  switch( eConstraint ){
    case P5_ConstraintNotNull:
      iMyErrno = 1048;
      break;
    case P5_ConstraintUnique:
      iMyErrno = 1062;
      break;
    case P5_ConstraintCheck:
      iMyErrno = 3819;
      zSqlState = "HY000";
      break;
    case P5_ConstraintFKChild:
      iMyErrno = 1452;
      break;
    case P5_ConstraintFKParent:
      iMyErrno = 1451;
      break;
    default:
      return;
  }
  sqlite3MyliteSetCondition(db, MYLITE_CONDITION_ERROR, iMyErrno, zSqlState);
}

void sqlite3MyliteClearCondition(sqlite3 *db){
  if( db==0 ) return;
  db->myliteCondition.eLevel = MYLITE_CONDITION_NONE;
  db->myliteCondition.iMyErrno = 0;
  db->myliteCondition.zSqlState[0] = 0;
}

void sqlite3MyliteClearColumnType(sqlite3 *db, Column *pCol){
  if( pCol==0 ) return;
  myliteClearColumnTypePayload(db, &pCol->myliteType);
  pCol->myliteType = (MyliteColumnType){0};
}

static int myliteSetColumnType(
  sqlite3 *db,
  const char *zSchema,
  const char *zTable,
  const char *zColumn,
  const MyliteColumnType *pType
){
  MyliteColumnType sqliteType = {0};
  char *zErr = 0;
  Table *pTab = 0;
  int iCol = 0;
  int rc = SQLITE_OK;

  if( zTable==0 || zTable[0]==0 || zColumn==0 || zColumn[0]==0 || pType==0 ){
    return SQLITE_MISUSE;
  }

  sqlite3_mutex_enter(db->mutex);
  rc = sqlite3Init(db, &zErr);
  if( rc==SQLITE_OK ){
    pTab = sqlite3FindTable(db, zTable, myliteSchemaName(zSchema));
    if( pTab==0 || pTab->aCol==0 ){
      rc = SQLITE_NOTFOUND;
    }
  }
  if( rc==SQLITE_OK ){
    iCol = sqlite3ColumnIndex(pTab, zColumn);
    if( iCol<0 ){
      rc = SQLITE_NOTFOUND;
    }
  }
  if( rc==SQLITE_OK ){
    rc = myliteCopyColumnType(db, &sqliteType, pType);
  }
  if( rc==SQLITE_OK ){
    sqlite3MyliteClearColumnType(db, &pTab->aCol[iCol]);
    pTab->aCol[iCol].myliteType = sqliteType;
    sqliteType = (MyliteColumnType){0};
    myliteRefreshTableTypeFlag(pTab);
  }
  sqlite3_mutex_leave(db->mutex);
  myliteClearColumnTypePayload(db, &sqliteType);
  sqlite3_free(zErr);
  return rc;
}

static int myliteSetEnumColumnType(
  sqlite3 *db,
  const char *zSchema,
  const char *zTable,
  const char *zColumn,
  const struct mylite_sqlite_fork_enum_column_type *pType
){
  MyliteColumnType sqliteType = {0};
  char *zErr = 0;
  Table *pTab = 0;
  int iCol = 0;
  int rc = SQLITE_OK;

  if( zTable==0 || zTable[0]==0 || zColumn==0 || zColumn[0]==0 ){
    return SQLITE_MISUSE;
  }

  sqlite3_mutex_enter(db->mutex);
  rc = sqlite3Init(db, &zErr);
  if( rc==SQLITE_OK ){
    pTab = sqlite3FindTable(db, zTable, myliteSchemaName(zSchema));
    if( pTab==0 || pTab->aCol==0 ){
      rc = SQLITE_NOTFOUND;
    }
  }
  if( rc==SQLITE_OK ){
    iCol = sqlite3ColumnIndex(pTab, zColumn);
    if( iCol<0 ){
      rc = SQLITE_NOTFOUND;
    }
  }
  if( rc==SQLITE_OK ){
    rc = myliteMakeEnumColumnType(db, pType, &sqliteType);
  }
  if( rc==SQLITE_OK ){
    sqlite3MyliteClearColumnType(db, &pTab->aCol[iCol]);
    pTab->aCol[iCol].myliteType = sqliteType;
    sqliteType = (MyliteColumnType){0};
    myliteRefreshTableTypeFlag(pTab);
  }
  sqlite3_mutex_leave(db->mutex);
  myliteClearColumnTypePayload(db, &sqliteType);
  sqlite3_free(zErr);
  return rc;
}

static int myliteSetSetColumnType(
  sqlite3 *db,
  const char *zSchema,
  const char *zTable,
  const char *zColumn,
  const struct mylite_sqlite_fork_set_column_type *pType
){
  MyliteColumnType sqliteType = {0};
  char *zErr = 0;
  Table *pTab = 0;
  int iCol = 0;
  int rc = SQLITE_OK;

  if( zTable==0 || zTable[0]==0 || zColumn==0 || zColumn[0]==0 ){
    return SQLITE_MISUSE;
  }

  sqlite3_mutex_enter(db->mutex);
  rc = sqlite3Init(db, &zErr);
  if( rc==SQLITE_OK ){
    pTab = sqlite3FindTable(db, zTable, myliteSchemaName(zSchema));
    if( pTab==0 || pTab->aCol==0 ){
      rc = SQLITE_NOTFOUND;
    }
  }
  if( rc==SQLITE_OK ){
    iCol = sqlite3ColumnIndex(pTab, zColumn);
    if( iCol<0 ){
      rc = SQLITE_NOTFOUND;
    }
  }
  if( rc==SQLITE_OK ){
    rc = myliteMakeSetColumnType(db, pType, &sqliteType);
  }
  if( rc==SQLITE_OK ){
    sqlite3MyliteClearColumnType(db, &pTab->aCol[iCol]);
    pTab->aCol[iCol].myliteType = sqliteType;
    sqliteType = (MyliteColumnType){0};
    myliteRefreshTableTypeFlag(pTab);
  }
  sqlite3_mutex_leave(db->mutex);
  myliteClearColumnTypePayload(db, &sqliteType);
  sqlite3_free(zErr);
  return rc;
}

static int myliteMakeColumnType(
  const struct mylite_sqlite_fork_column_type *pType,
  MyliteColumnType *pOut
){
  *pOut = (MyliteColumnType){0};

  switch( pType->kind ){
    case MYLITE_SQLITE_FORK_COLUMN_TYPE_NONE:
      pOut->eType = MYLITE_COLTYPE_NONE;
      return SQLITE_OK;
    case MYLITE_SQLITE_FORK_COLUMN_TYPE_SIGNED_INTEGER:
      if( pType->integer_minimum>pType->integer_maximum ) return SQLITE_MISUSE;
      pOut->eType = MYLITE_COLTYPE_SIGNED_INTEGER;
      pOut->iMin = pType->integer_minimum;
      pOut->iMax = pType->integer_maximum;
      return SQLITE_OK;
    case MYLITE_SQLITE_FORK_COLUMN_TYPE_UNSIGNED_INTEGER:
      if( pType->integer_maximum<0 ) return SQLITE_MISUSE;
      pOut->eType = MYLITE_COLTYPE_UNSIGNED_INTEGER;
      pOut->iMin = 0;
      pOut->iMax = pType->integer_maximum;
      return SQLITE_OK;
    case MYLITE_SQLITE_FORK_COLUMN_TYPE_DOUBLE:
      pOut->eType = MYLITE_COLTYPE_DOUBLE;
      return SQLITE_OK;
    case MYLITE_SQLITE_FORK_COLUMN_TYPE_VARCHAR:
      pOut->eType = MYLITE_COLTYPE_VARCHAR;
      pOut->nChar = pType->character_maximum_length;
      return SQLITE_OK;
    case MYLITE_SQLITE_FORK_COLUMN_TYPE_BINARY:
      pOut->eType = MYLITE_COLTYPE_BINARY;
      pOut->nByte = pType->byte_maximum_length;
      return SQLITE_OK;
    case MYLITE_SQLITE_FORK_COLUMN_TYPE_VARBINARY:
      pOut->eType = MYLITE_COLTYPE_VARBINARY;
      pOut->nByte = pType->byte_maximum_length;
      return SQLITE_OK;
    case MYLITE_SQLITE_FORK_COLUMN_TYPE_DECIMAL:
      if( pType->numeric_precision==0 || pType->numeric_precision>65 ||
          pType->numeric_scale>30 ||
          pType->numeric_scale>pType->numeric_precision ){
        return SQLITE_MISUSE;
      }
      if( pType->flags & ~MYLITE_SQLITE_FORK_COLUMN_TYPE_UNSIGNED ){
        return SQLITE_MISUSE;
      }
      pOut->eType = MYLITE_COLTYPE_DECIMAL;
      pOut->nPrecision = (u8)pType->numeric_precision;
      pOut->nScale = (u8)pType->numeric_scale;
      if( pType->flags & MYLITE_SQLITE_FORK_COLUMN_TYPE_UNSIGNED ){
        pOut->mFlags |= MYLITE_COLTYPE_FLAG_UNSIGNED;
      }
      return SQLITE_OK;
    case MYLITE_SQLITE_FORK_COLUMN_TYPE_DATE:
      if( pType->flags & ~MYLITE_SQLITE_FORK_COLUMN_TYPE_ALLOW_ZERO_TEMPORAL ){
        return SQLITE_MISUSE;
      }
      if( pType->datetime_precision!=0 ) return SQLITE_MISUSE;
      pOut->eType = MYLITE_COLTYPE_DATE;
      if( pType->flags & MYLITE_SQLITE_FORK_COLUMN_TYPE_ALLOW_ZERO_TEMPORAL ){
        pOut->mFlags |= MYLITE_COLTYPE_FLAG_ALLOW_ZERO;
      }
      return SQLITE_OK;
    case MYLITE_SQLITE_FORK_COLUMN_TYPE_DATETIME:
      if( pType->flags & ~MYLITE_SQLITE_FORK_COLUMN_TYPE_ALLOW_ZERO_TEMPORAL ){
        return SQLITE_MISUSE;
      }
      if( pType->datetime_precision>6 ) return SQLITE_MISUSE;
      pOut->eType = MYLITE_COLTYPE_DATETIME;
      pOut->nFsp = (u8)pType->datetime_precision;
      if( pType->flags & MYLITE_SQLITE_FORK_COLUMN_TYPE_ALLOW_ZERO_TEMPORAL ){
        pOut->mFlags |= MYLITE_COLTYPE_FLAG_ALLOW_ZERO;
      }
      return SQLITE_OK;
    case MYLITE_SQLITE_FORK_COLUMN_TYPE_TIMESTAMP:
      if( pType->flags & ~MYLITE_SQLITE_FORK_COLUMN_TYPE_ALLOW_ZERO_TEMPORAL ){
        return SQLITE_MISUSE;
      }
      if( pType->datetime_precision>6 ) return SQLITE_MISUSE;
      pOut->eType = MYLITE_COLTYPE_TIMESTAMP;
      pOut->nFsp = (u8)pType->datetime_precision;
      if( pType->flags & MYLITE_SQLITE_FORK_COLUMN_TYPE_ALLOW_ZERO_TEMPORAL ){
        pOut->mFlags |= MYLITE_COLTYPE_FLAG_ALLOW_ZERO;
      }
      return SQLITE_OK;
    case MYLITE_SQLITE_FORK_COLUMN_TYPE_TIME:
      if( pType->flags!=0 ) return SQLITE_MISUSE;
      if( pType->datetime_precision>6 ) return SQLITE_MISUSE;
      pOut->eType = MYLITE_COLTYPE_TIME;
      pOut->nFsp = (u8)pType->datetime_precision;
      return SQLITE_OK;
    case MYLITE_SQLITE_FORK_COLUMN_TYPE_TEXT:
      if( pType->flags!=0 ) return SQLITE_MISUSE;
      pOut->eType = MYLITE_COLTYPE_TEXT;
      pOut->nByte = pType->byte_maximum_length;
      return SQLITE_OK;
    case MYLITE_SQLITE_FORK_COLUMN_TYPE_BLOB:
      if( pType->flags!=0 ) return SQLITE_MISUSE;
      pOut->eType = MYLITE_COLTYPE_BLOB;
      pOut->nByte = pType->byte_maximum_length;
      return SQLITE_OK;
    case MYLITE_SQLITE_FORK_COLUMN_TYPE_YEAR:
      if( pType->flags!=0 ) return SQLITE_MISUSE;
      pOut->eType = MYLITE_COLTYPE_YEAR;
      return SQLITE_OK;
    case MYLITE_SQLITE_FORK_COLUMN_TYPE_BIT:
      if( pType->flags!=0 ) return SQLITE_MISUSE;
      if( pType->numeric_precision<1 || pType->numeric_precision>64 ){
        return SQLITE_MISUSE;
      }
      pOut->eType = MYLITE_COLTYPE_BIT;
      pOut->nPrecision = (u8)pType->numeric_precision;
      return SQLITE_OK;
    case MYLITE_SQLITE_FORK_COLUMN_TYPE_JSON:
      if( pType->flags!=0 ) return SQLITE_MISUSE;
      pOut->eType = MYLITE_COLTYPE_JSON;
      return SQLITE_OK;
    case MYLITE_SQLITE_FORK_COLUMN_TYPE_ENUM:
      return SQLITE_MISUSE;
    case MYLITE_SQLITE_FORK_COLUMN_TYPE_SET:
      return SQLITE_MISUSE;
  }

  return SQLITE_MISUSE;
}

static int myliteMakeEnumColumnType(
  sqlite3 *db,
  const struct mylite_sqlite_fork_enum_column_type *pType,
  MyliteColumnType *pOut
){
  if( pType->flags!=0 ) return SQLITE_MISUSE;
  return myliteMakeValueListColumnType(
      db, pType->values, pType->value_count, 65535, MYLITE_COLTYPE_ENUM, 0, pOut
  );
}

static int myliteMakeSetColumnType(
  sqlite3 *db,
  const struct mylite_sqlite_fork_set_column_type *pType,
  MyliteColumnType *pOut
){
  if( pType->flags!=0 ) return SQLITE_MISUSE;
  return myliteMakeValueListColumnType(
      db, pType->values, pType->value_count, 64, MYLITE_COLTYPE_SET, 1, pOut
  );
}

static int myliteMakeValueListColumnType(
  sqlite3 *db,
  const struct mylite_sqlite_fork_enum_value *aValue,
  sqlite3_uint64 nValue,
  sqlite3_uint64 nMaxValue,
  u8 eType,
  int bRejectComma,
  MyliteColumnType *pOut
){
  sqlite3_uint64 i;
  sqlite3_uint64 j;

  *pOut = (MyliteColumnType){0};
  if( aValue==0 || nValue==0 || nValue>nMaxValue ){
    return SQLITE_MISUSE;
  }
  for(i=0; i<nValue; i++){
    const struct mylite_sqlite_fork_enum_value *pValue = &aValue[i];
    if( pValue->text==0 || pValue->byte_length>0x7fffffff ){
      return SQLITE_MISUSE;
    }
    if( bRejectComma &&
        memchr(pValue->text, ',', (size_t)pValue->byte_length)!=0 ){
      return SQLITE_MISUSE;
    }
    for(j=0; j<i; j++){
      const struct mylite_sqlite_fork_enum_value *pPrior = &aValue[j];
      if( pPrior->byte_length==pValue->byte_length &&
          memcmp(pPrior->text, pValue->text, (size_t)pValue->byte_length)==0 ){
        return SQLITE_MISUSE;
      }
    }
  }

  pOut->eType = eType;
  pOut->nValue = (u32)nValue;
  pOut->aValue = sqlite3DbMallocZero(0, sizeof(pOut->aValue[0])*pOut->nValue);
  if( pOut->aValue==0 ) return SQLITE_NOMEM;
  for(i=0; i<nValue; i++){
    const struct mylite_sqlite_fork_enum_value *pValue = &aValue[i];
    u32 n = (u32)pValue->byte_length;
    pOut->aValue[i].z = sqlite3DbMallocRaw(0, n+1);
    if( pOut->aValue[i].z==0 ){
      myliteClearColumnTypePayload(db, pOut);
      return SQLITE_NOMEM;
    }
    if( n>0 ){
      memcpy(pOut->aValue[i].z, pValue->text, n);
    }
    pOut->aValue[i].z[n] = 0;
    pOut->aValue[i].n = n;
  }
  return SQLITE_OK;
}

static int myliteCopyColumnType(
  sqlite3 *db,
  MyliteColumnType *pOut,
  const MyliteColumnType *pIn
){
  u32 i;

  *pOut = *pIn;
  pOut->aValue = 0;
  pOut->nValue = 0;
  if( pIn->nValue==0 ){
    return SQLITE_OK;
  }
  pOut->aValue = sqlite3DbMallocZero(0, sizeof(pOut->aValue[0])*pIn->nValue);
  if( pOut->aValue==0 ) return SQLITE_NOMEM;
  pOut->nValue = pIn->nValue;
  for(i=0; i<pIn->nValue; i++){
    pOut->aValue[i].z = sqlite3DbMallocRaw(0, pIn->aValue[i].n+1);
    if( pOut->aValue[i].z==0 ){
      myliteClearColumnTypePayload(db, pOut);
      return SQLITE_NOMEM;
    }
    if( pIn->aValue[i].n>0 ){
      memcpy(pOut->aValue[i].z, pIn->aValue[i].z, pIn->aValue[i].n);
    }
    pOut->aValue[i].z[pIn->aValue[i].n] = 0;
    pOut->aValue[i].n = pIn->aValue[i].n;
  }
  return SQLITE_OK;
}

static void myliteClearColumnTypePayload(sqlite3 *db, MyliteColumnType *pType){
  u32 i;
  (void)db;
  if( pType==0 || pType->aValue==0 ) return;
  for(i=0; i<pType->nValue; i++){
    sqlite3_free(pType->aValue[i].z);
  }
  sqlite3_free(pType->aValue);
  pType->aValue = 0;
  pType->nValue = 0;
}

static const char *myliteSchemaName(const char *zSchema){
  if( zSchema==0 || zSchema[0]==0 ) return 0;
  return zSchema;
}

static void myliteRefreshTableTypeFlag(Table *pTab){
  int i;
  pTab->tabFlags &= (u32)~(u32)TF_MyliteTypes;
  for(i=0; i<pTab->nCol; i++){
    if( pTab->aCol[i].myliteType.eType!=MYLITE_COLTYPE_NONE ){
      pTab->tabFlags |= TF_MyliteTypes;
      return;
    }
  }
}
