#ifndef _WALDB_H
#define _WALDB_H
#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief Database Error codes.
 */
typedef enum
{
	DB_SUCCESS = 0,                    /**< Success. */
	DB_FAILURE,                        /**< General Failure */
	DB_ERR_WILDCARD_NOT_SUPPORTED,
	DB_ERR_INVALID_PARAMETER,
	DB_ERR_TIMEOUT,
	DB_ERR_NOT_EXIST
}
DB_STATUS;

/* @brief Loads the data-model xml data
 *
 * @filename[in] data-model xml filename (with absolute path)
 * @dbhandle[out] database handle
 * @return DB_STATUS
 */
DB_STATUS loaddb(const char *filename,void *dbhandle);

/* @brief Returns a parameter list and count given an input paramName with wildcard characters
 *
 * @dbhandle[in] database handle to query in to
 * @paramName[in] parameter name with wildcard(*)
 * @ParamList[out] parameter list extended by the input parameter
 * @ParamDataTypeList[out] parameter data type list extended by the input wildcard parameter
 * @paramCount[out] parameter count
 * @return DB_STATUS
 */
DB_STATUS getParameterList(void *dbhandle,char *paramName,char **ParamList,char **ParamDataTypeList,int *paramCount);

/* @brief Returns a parameter list and count given an input paramName with wildcard characters
 *
 * @filename[in] data-model xml filename (with absolute path)
 * @dbhandle[out] database handle
 * @dataType[out] Parameter DataType output
 * @return DB_STATUS
 */
int isParameterValid(void *handle,char *paramName,char *dataType);
#ifdef __cplusplus
}
#endif
#endif /*_WALDB_H*/

