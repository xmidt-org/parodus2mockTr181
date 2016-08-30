#include <iostream>
#include <string>
#include "waldb/waldb.h"
#include "tinyxml.h"
#include "stdlib.h"

#define MAX_PARAMETER_LENGTH 512
#define MAX_DATATYPE_LENGTH 48
#define MAX_NUM_PARAMETERS 2048

static void getList(TiXmlNode *pParent,char *paramName,char **ptrParamList,char **pParamDataTypeList,int *paramCount);
static void checkforParameterMatch(TiXmlNode *pParent,char *paramName,int *pMatch,char *dataType);

/* @brief Loads the data-model xml data
 *
 * @filename[in] data-model xml filename (with absolute path)
 * @dbhandle[out] database handle
 * @return DB_STATUS
 */
DB_STATUS loaddb(const char *filename,void *dbhandle)
{
	TiXmlDocument *doc = NULL;
	int *dbhdl = (int *)dbhandle;
	doc = new TiXmlDocument(filename);
	if(doc != NULL)
	{
		bool loadOK = doc->LoadFile();
		if( loadOK )
		{
			*dbhdl = (int) doc;
			return DB_SUCCESS;
		}
		else
		{
			*dbhdl = 0;
			return DB_FAILURE;
		}
	}
	else
	{
		*dbhdl = 0;
		return DB_FAILURE;
	}
}

static void getList(TiXmlNode *pParent,char *paramName,char **ptrParamList,char **pParamDataTypeList,int *paramCount)
{
	static int matched = 0;
	if(!pParent)
	{
		matched = 0;
		return;
	}

	TiXmlNode* pChild;

	static int isObject = 0;
	static char ObjectName[MAX_PARAMETER_LENGTH];
	char ParameterName[MAX_PARAMETER_LENGTH];
	if(pParent->Type() == TiXmlNode::TINYXML_ELEMENT)
	{
		TiXmlElement* pElement =  pParent->ToElement();
		TiXmlAttribute* pAttrib = pElement->FirstAttribute();
		if(!strcmp(pParent->Value(),"object"))
		{
			isObject = 1;
		}
		if(pAttrib)
		{
			if(strstr(pAttrib->Value(),paramName))
			{
				strncpy(ObjectName,pAttrib->Value(),MAX_PARAMETER_LENGTH-1);
				ObjectName[MAX_PARAMETER_LENGTH-1]='\0';
				matched = 1;
			}
			if(matched || !isObject)
			{
				if(!strcmp(pParent->Value(),"parameter"))
				{
					isObject = 0;
					if(*paramCount <= MAX_NUM_PARAMETERS)
					{
						ptrParamList[*paramCount] = (char *) malloc(MAX_PARAMETER_LENGTH * sizeof(char));
						pParamDataTypeList[*paramCount] = (char *) malloc(MAX_DATATYPE_LENGTH * sizeof(char));

						strncpy(ptrParamList[*paramCount],ObjectName,MAX_PARAMETER_LENGTH-1);
						strncat(ptrParamList[*paramCount],pAttrib->Value(),MAX_PARAMETER_LENGTH-1);
						ptrParamList[*paramCount][MAX_PARAMETER_LENGTH-1]='\0';

						strncpy(pParamDataTypeList[*paramCount],pParent->FirstChild()->FirstChild()->Value(),MAX_DATATYPE_LENGTH-1);
						pParamDataTypeList[*paramCount][MAX_DATATYPE_LENGTH-1]='\0';
					}
					*paramCount = *paramCount + 1;
				}
			}
		}
	}

	for ( pChild = pParent->FirstChild(); pChild != 0; pChild = pChild->NextSibling())
	{
		getList(pChild,paramName,ptrParamList,pParamDataTypeList,paramCount);
	}
	matched = 0;
}
/* @brief Returns a parameter list and count given an input paramName with wildcard characters
 *
 * @dbhandle[in] database handle to query in to
 * @paramName[in] parameter name with wildcard(*)
 * @ParamList[out] parameter list extended by the input wildcard parameter
 * @ParamDataTypeList[out] parameter data type list extended by the input wildcard parameter
 * @paramCount[out] parameter count
 * @return DB_STATUS
 */
DB_STATUS getParameterList(void *dbhandle,char *paramName,char **ParamList,char **ParamDataTypeList,int *paramCount)
{
	const char wcard = '*';  //This API currently supports only paramName with * wildcard
	char parameterName[MAX_PARAMETER_LENGTH];
	strncpy(parameterName,paramName,MAX_PARAMETER_LENGTH-1);
	parameterName[MAX_PARAMETER_LENGTH-1]='\0';

	TiXmlDocument *doc = (TiXmlDocument *) dbhandle;
	if(strchr(parameterName,wcard))
	{
		// Remove the wildcard(*) from parameterName to populate list
		char *ptr = strchr(parameterName,wcard);
		*ptr = '\0';

		getList(doc,parameterName,ParamList,ParamDataTypeList,paramCount);

		if(*paramCount == 0)
		{
			return DB_ERR_INVALID_PARAMETER;
		}
	}
	else
	{
		return DB_ERR_WILDCARD_NOT_SUPPORTED;
	}

	return DB_SUCCESS;
}

static void checkforParameterMatch(TiXmlNode *pParent,char *paramName,int *pMatch,char *dataType)
{
	static int matched = 0;
	if(!pParent)
		return;

	TiXmlNode *pChild;
	static int isObject = 0;
	static char ObjectName[MAX_PARAMETER_LENGTH];
	char *ParameterName = (char *) malloc(sizeof(char) * MAX_PARAMETER_LENGTH);

	if(pParent->Type() == TiXmlNode::TINYXML_ELEMENT)
	{
		TiXmlElement* pElement = pParent->ToElement();
		TiXmlAttribute* pAttrib = pElement->FirstAttribute();
		if(!strcmp(pParent->Value(),"object"))
			isObject = 1;

		if(pAttrib)
		{
			// Construct Object without parameter from input ParamName
			std::string *str1 = new std::string(paramName);
			std::size_t found = str1->find_last_of(".");
			char *paramObject = (char *) malloc(sizeof(char) * MAX_PARAMETER_LENGTH);
			if(found != std::string::npos)
			{
				strncpy(paramObject,paramName,found);
				paramObject[found]='.';
				paramObject[found+1]='\0';
			}
			else
			{
				strncpy(paramObject,paramName,MAX_PARAMETER_LENGTH-1);
				paramObject[MAX_PARAMETER_LENGTH] = '\0';
			}

			free(str1);
			if(!strcmp(pAttrib->Value(),paramObject))
			{
				strncpy(ObjectName,pAttrib->Value(),MAX_PARAMETER_LENGTH-1);
				ObjectName[MAX_PARAMETER_LENGTH] = '\0';
				matched = 1;
			}
			free(paramObject);

			if(matched || !isObject)
			{
				if(!strcmp(pParent->Value(),"parameter"))
				{
					isObject = 0;
					strncpy(ParameterName,ObjectName,MAX_PARAMETER_LENGTH-1);
					strncat(ParameterName,pAttrib->Value(),MAX_PARAMETER_LENGTH-1);
					ParameterName[MAX_PARAMETER_LENGTH] = '\0';
					if(!strcmp(ParameterName,paramName))
					{
						strncpy(dataType,pParent->FirstChild()->FirstChild()->Value(),MAX_DATATYPE_LENGTH-1);
						*pMatch = 1;
						return;
					}
				}
			}
		}
	}

	free(ParameterName);

	for ( pChild = pParent->FirstChild(); pChild != 0; pChild = pChild->NextSibling())
	{
		checkforParameterMatch(pChild,paramName,pMatch,dataType);
	}
	matched = 0;
}

/* @brief Returns a parameter list and count given an input paramName with wildcard characters
 *
 * @filename[in] data-model xml filename (with absolute path)
 * @dbhandle[out] database handle
 * @dataType[out] Parameter DataType output
 * @return DB_STATUS
 */
int isParameterValid(void *dbhandle,char *paramName,char *dataType)
{
	int Match = 0;
	int first_i = 0;
	TiXmlDocument *doc = (TiXmlDocument *) dbhandle;

	/* Check if Parameter is one of {i} entriesi ex:Device.WiFi.Radio.1.Status should become Device.WiFi.Radio.{i}.Status */
	std::string str(paramName);
	std::size_t found = str.find_first_of("0123456789");
	if(found != std::string::npos)
	{
        	/* Check if match happens without a {i} */
        	checkforParameterMatch(doc,paramName,&Match,dataType);
        	if(Match)
            		return true;
		first_i = found+1;
		char *newparamName =(char *) malloc(sizeof(char) * MAX_PARAMETER_LENGTH);
		char splitParam[MAX_PARAMETER_LENGTH] = "{i}";
		if(paramName[found+1] == '.')
		{
			strncpy(newparamName,paramName,found);
			newparamName[found]='\0';
			strncat(splitParam,str.substr(found+1).data(),MAX_PARAMETER_LENGTH-1);
			splitParam[MAX_PARAMETER_LENGTH-1]='\0';
		        // Check for Parameter Match with first {i}
		        strncat(newparamName,splitParam,MAX_PARAMETER_LENGTH-1);
		        newparamName[MAX_PARAMETER_LENGTH-1]='\0';
		        checkforParameterMatch(doc,newparamName,&Match,dataType);
		        if(Match)
       			     return true;
		}
		else
		{
                	strncpy(newparamName,paramName,found+1);
                	newparamName[found+1]='\0';
                	strncpy(splitParam,paramName+found+1,MAX_PARAMETER_LENGTH);
			splitParam[MAX_PARAMETER_LENGTH-1]='\0';
		}

		// Check if splitParam has a digit
		std::string str(splitParam);
		std::size_t found = str.find_first_of("0123456789");
		if(found != std::string::npos)
		{
			strncpy(newparamName,paramName,first_i);
            		newparamName[first_i] = '\0';

               		if(splitParam[found+1]=='.')
                	{
				splitParam[found]='\0';
				strcat(splitParam,"{i}");
				strncat(splitParam,str.substr(found+1).data(),MAX_PARAMETER_LENGTH-1);
				splitParam[MAX_PARAMETER_LENGTH-1]='\0';

			        strncat(newparamName,splitParam+3,MAX_PARAMETER_LENGTH-1);
            			newparamName[MAX_PARAMETER_LENGTH-1]='\0';
            			checkforParameterMatch(doc,newparamName,&Match,dataType);
            			if(Match)
               				return true;
			}
			else
			{
		                /* Find if there are more {i} entries */
		                int first_num = found;
		                std::string str(splitParam+first_num+1);
		                std::size_t found = str.find_first_of("0123456789");
		                if(found != std::string::npos)
		                {
		                    splitParam[found+first_num]='\0';
		                    strcat(splitParam,".{i}");
		                    strncat(splitParam,str.substr(found+1).data(),MAX_PARAMETER_LENGTH-1);
		                    splitParam[MAX_PARAMETER_LENGTH-1]='\0';
		                    //Check for parameter match with second {i}
		                    strncat(newparamName,splitParam+3,MAX_PARAMETER_LENGTH-1);
		                    newparamName[MAX_PARAMETER_LENGTH-1]='\0';
		                    checkforParameterMatch(doc,newparamName,&Match,dataType);
		                    if(Match)
		                        return true;
				}
			}
		}

		if(first_i)
	        	newparamName[first_i-1]='\0';	
		strncat(newparamName,splitParam,MAX_PARAMETER_LENGTH-1);
		newparamName[MAX_PARAMETER_LENGTH-1]='\0';
		checkforParameterMatch(doc,newparamName,&Match,dataType);
		free(newparamName);
	}
	else
	{
		checkforParameterMatch(doc,paramName,&Match,dataType);
	}
	if(Match)
		return 1;
	else
		return 0;
}
