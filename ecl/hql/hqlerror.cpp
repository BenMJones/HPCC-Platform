/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2012 HPCC Systems.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
############################################################################## */

#include "jlog.hpp"
#include "hqlerror.hpp"
#include "hqlerrors.hpp"

class HQL_API CECLError : public CInterfaceOf<IECLError>
{
public:
    CECLError(ErrorSeverity _severity, int _no, const char* _msg, const char* _filename, int _lineno, int _column, int _position);

    virtual int             errorCode() const { return no; }
    virtual StringBuffer &  errorMessage(StringBuffer & ret) const { return ret.append(msg); }
    virtual MessageAudience errorAudience() const { return MSGAUD_user; }
    virtual const char* getFilename() const { return filename; }
    virtual WarnErrorCategory getCategory() const { return WECunknown; }
    virtual int getLine() const { return lineno; }
    virtual int getColumn() const { return column; }
    virtual int getPosition() const { return position; }
    virtual StringBuffer& toString(StringBuffer&) const;
    virtual ErrorSeverity getSeverity() const { return severity; }
    virtual IECLError * cloneSetSeverity(ErrorSeverity _severity) const;

protected:
    ErrorSeverity severity;
    int no;
    StringAttr msg;
    StringAttr filename;
    int lineno;
    int column;
    int position;
};

CECLError::CECLError(ErrorSeverity _severity, int _no, const char* _msg, const char* _filename, int _lineno, int _column, int _position):
  severity(_severity), msg(_msg), filename(_filename)
{
    no = _no;
    lineno = _lineno;
    column = _column;
    position = _position;
}


StringBuffer& CECLError::toString(StringBuffer& buf) const
{
    buf.append(filename);

    if(lineno && column)
        buf.append('(').append(lineno).append(',').append(column).append(')');
    buf.append(" : ");

    buf.append(no).append(": ").append(msg);
    return buf;
}

IECLError * CECLError::cloneSetSeverity(ErrorSeverity newSeverity) const
{
    return new CECLError(newSeverity,
                         errorCode(), msg, filename,
                         getLine(), getColumn(), getPosition());
}

extern HQL_API IECLError *createECLError(ErrorSeverity severity, int errNo, const char *msg, const char * filename, int lineno, int column, int pos)
{
    return new CECLError(severity,errNo,msg,filename,lineno,column,pos);
}

//---------------------------------------------------------------------------------------------------------------------

void IErrorReceiver::reportError(int errNo, const char *msg, const char *filename, int lineno, int column, int position)
{
    Owned<IECLError> err = createECLError(errNo,msg,filename,lineno,column,position);
    report(err);
}

void IErrorReceiver::reportWarning(int warnNo, const char *msg, const char *filename, int lineno, int column, int position)
{
    Owned<IECLError> warn = createECLError(SeverityWarning,warnNo,msg,filename,lineno,column,position);
    report(warn);
}

//---------------------------------------------------------------------------------------------------------------------

void ErrorReceiverSink::report(IECLError* error)
{
    switch (error->getSeverity())
    {
    case SeverityWarning:
        warns++;
        break;
    case SeverityError:
    case SeverityFatal:
        errs++;
        break;
    }
}

//---------------------------------------------------------------------------------------------------------------------



void MultiErrorReceiver::report(IECLError* error)
{
    ErrorReceiverSink::report(error);

    msgs.append(*LINK(error));

    StringBuffer msg;
    DBGLOG("reportError(%d:%d) : %s", error->getLine(), error->getColumn(), error->errorMessage(msg).str());
}

StringBuffer& MultiErrorReceiver::toString(StringBuffer& buf)
{
    ForEachItemIn(i, msgs)
    {
        IECLError* error=item(i);
        error->toString(buf);
        buf.append('\n');
    }
    return buf;
}

IECLError *MultiErrorReceiver::firstError()
{
    ForEachItemIn(i, msgs)
    {
        IECLError* error = item(i);
        if (isError(error->getSeverity()))
            return error;
    }
    return NULL;
}

extern HQL_API void reportErrors(IErrorReceiver & receiver, IECLErrorArray & errors)
{
    ForEachItemIn(i, errors)
        receiver.report(&errors.item(i));
}

//---------------------------------------------------------------------------------------------------------------------

class HQL_API FileErrorReceiver : public ErrorReceiverSink
{
public:
    FileErrorReceiver(FILE *_f)
    {
        f = _f;
    }

    virtual void report(IECLError* error)
    {
        ErrorReceiverSink::report(error);

        ErrorSeverity severity = error->getSeverity();
        if (severity <= SeverityInfo)
            return;

        unsigned code = error->errorCode();
        const char * filename = error->getFilename();
        unsigned line = error->getLine();
        unsigned column = error->getColumn();
        unsigned position = error->getPosition();

        StringBuffer msg;
        error->errorMessage(msg);
        if (isError(severity))
        {
            if (!filename) filename = "";
            fprintf(f, "%s(%d,%d): error C%04d: %s\n", filename, line, column, code, msg.str());
        }
        else
        {
            if (!filename) filename = *unknownAtom;
            fprintf(f, "%s(%d,%d): warning C%04d: %s\n", filename, line, column, code, msg.str());
        }
    }

protected:
    FILE *f;
};

extern HQL_API IErrorReceiver *createFileErrorReceiver(FILE *f)
{
    return new FileErrorReceiver(f);
}

//---------------------------------------------------------------------------------------------------------------------

class HQL_API ThrowingErrorReceiver : public ErrorReceiverSink
{
    virtual void report(IECLError* error);
};

void ThrowingErrorReceiver::report(IECLError* error)
{
    throw error;
}

IErrorReceiver * createThrowingErrorReceiver()
{
    return new ThrowingErrorReceiver;
}

//---------------------------------------------------------------------------------------------------------------------

class HQL_API NullErrorReceiver : public ErrorReceiverSink
{
public:
};

IErrorReceiver * createNullErrorReceiver()
{
    return new NullErrorReceiver;
}

//---------------------------------------------------------------------------------------------------------------------

void reportErrorVa(IErrorReceiver * errors, int errNo, const ECLlocation & loc, const char* format, va_list args)
{
    StringBuffer msg;
    msg.valist_appendf(format, args);
    if (errors)
        errors->reportError(errNo, msg.str(), loc.sourcePath->str(), loc.lineno, loc.column, loc.position);
    else
        throw createECLError(errNo, msg.str(), loc.sourcePath->str(), loc.lineno, loc.column, loc.position);
}

void reportError(IErrorReceiver * errors, int errNo, const ECLlocation & loc, const char * format, ...)
{
    va_list args;
    va_start(args, format);
    reportErrorVa(errors, errNo, loc, format, args);
    va_end(args);
}


class ErrorInserter : public IndirectErrorReceiver
{
public:
    ErrorInserter(IErrorReceiver & _prev, IECLError * _error) : IndirectErrorReceiver(_prev), error(_error) {}

    virtual void report(IECLError* error)
    {
        if (isError(error->getSeverity()))
            flush();
        IndirectErrorReceiver::report(error);
    }

protected:
    void flush()
    {
        if (error)
        {
            IndirectErrorReceiver::report(error);
            error.clear();
        }
    }

protected:
    Linked<IECLError> error;
};

//---------------------------------------------------------------------------------------------------------------------

class AbortingErrorReceiver : public IndirectErrorReceiver
{
public:
    AbortingErrorReceiver(IErrorReceiver & _prev) : IndirectErrorReceiver(_prev) {}

    virtual void report(IECLError* error)
    {
        Owned<IECLError> mappedError = prevErrorProcessor->mapError(error);
        prevErrorProcessor->report(mappedError);
        if (isError(mappedError->getSeverity()))
            throw MakeStringExceptionDirect(HQLERR_ErrorAlreadyReported, "");
    }
};

IErrorReceiver * createAbortingErrorReceiver(IErrorReceiver & prev)
{
    return new AbortingErrorReceiver(prev);
}

//---------------------------------------------------------------------------------------------------------------------

class DedupingErrorReceiver : public IndirectErrorReceiver
{
public:
    DedupingErrorReceiver(IErrorReceiver & _prev) : IndirectErrorReceiver(_prev) {}

    virtual void report(IECLError* error)
    {
        if (errors.contains(*error))
            return;
        errors.append(*LINK(error));
        IndirectErrorReceiver::report(error);
    }

private:
    Array errors;
};

IErrorReceiver * createDedupingErrorReceiver(IErrorReceiver & prev)
{
    return new DedupingErrorReceiver(prev);
}

//---------------------------------------------------------------------------------------------------------------------

void checkEclVersionCompatible(Shared<IErrorReceiver> & errors, const char * eclVersion)
{
    if (eclVersion)
    {
        unsigned major, minor, subminor;
        if (extractVersion(major, minor, subminor, eclVersion))
        {
            if (major != LANGUAGE_VERSION_MAJOR)
            {
                VStringBuffer msg("Mismatch in major version number (%s v %s)", eclVersion, LANGUAGE_VERSION);
                errors->reportWarning(HQLERR_VersionMismatch, msg.str(), NULL, 0, 0, 0);
            }
            else if (minor != LANGUAGE_VERSION_MINOR)
            {
                VStringBuffer msg("Mismatch in minor version number (%s v %s)", eclVersion, LANGUAGE_VERSION);
                errors->reportWarning(HQLERR_VersionMismatch, msg.str(), NULL, 0, 0, 0);
            }
            else if (subminor != LANGUAGE_VERSION_SUB)
            {
                //This adds the warning if any other warnings occur.
                VStringBuffer msg("Mismatch in subminor version number (%s v %s)", eclVersion, LANGUAGE_VERSION);
                Owned<IECLError> warning = createECLError(SeverityWarning, HQLERR_VersionMismatch, msg.str(), NULL, 0, 0);
                errors.setown(new ErrorInserter(*errors, warning));
            }
        }
    }
}
