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
static int myliteMakeColumnType(
  const struct mylite_sqlite_fork_column_type *pType,
  MyliteColumnType *pOut
);
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
  const MyliteColumnType sqliteType = {
    MYLITE_COLTYPE_NONE, 0, 0, 0, 0, 0, 0, 0, 0
  };

  if( db==0 ) return SQLITE_MISUSE;
  return myliteSetColumnType(db, zSchema, zTable, zColumn, &sqliteType);
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

void sqlite3MyliteClearCondition(sqlite3 *db){
  if( db==0 ) return;
  db->myliteCondition.eLevel = MYLITE_CONDITION_NONE;
  db->myliteCondition.iMyErrno = 0;
  db->myliteCondition.zSqlState[0] = 0;
}

static int myliteSetColumnType(
  sqlite3 *db,
  const char *zSchema,
  const char *zTable,
  const char *zColumn,
  const MyliteColumnType *pType
){
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
    pTab->aCol[iCol].myliteType = *pType;
    myliteRefreshTableTypeFlag(pTab);
  }
  sqlite3_mutex_leave(db->mutex);
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
  }

  return SQLITE_MISUSE;
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
