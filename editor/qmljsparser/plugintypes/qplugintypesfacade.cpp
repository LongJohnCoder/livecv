#include "qplugintypesfacade.h"

#include <QtQml/private/qqmlmetatype_p.h>
#include <QtQml/private/qqmlopenmetaobject_p.h>
#include <QtQuick/private/qquickevents_p_p.h>
#include <QtQuick/private/qquickpincharea_p.h>
#include <QtCore/private/qobject_p.h>
#include <QtCore/private/qmetaobject_p.h>


#include <QtQml/private/qhashedstring_p.h>

#include "qmlstreamwriter.h"

#include <iostream>
#include <algorithm>

#include <QDebug>

namespace lcv{

namespace{

//QString pluginImportPath;
//bool verbose = false;
//bool creatable = true;

//QString currentProperty;
//QString inObjectInstantiation;

static QString enquote(const QString &string)
{
    QString s = string;
    return QString("\"%1\"").arg(s.replace(QLatin1Char('\\'), QLatin1String("\\\\"))
                                 .replace(QLatin1Char('"'),QLatin1String("\\\"")));
}

//void collectReachableMetaObjects(const QMetaObject *meta, QSet<const QMetaObject *> *metas, bool extended = false)
//{
//    if (! meta || metas->contains(meta))
//        return;

//    // dynamic meta objects can break things badly
//    // but extended types are usually fine
//    const QMetaObjectPrivate *mop = reinterpret_cast<const QMetaObjectPrivate *>(meta->d.data);
//    if (extended || !(mop->flags & DynamicMetaObject))
//        metas->insert(meta);

//    collectReachableMetaObjects(meta->superClass(), metas);
//}

//void collectReachableMetaObjects(QObject *object, QSet<const QMetaObject *> *metas)
//{
//    if (! object)
//        return;

//    const QMetaObject *meta = object->metaObject();
//    if (verbose)
//        std::cerr << "Processing object" << qPrintable( meta->className() ) << std::endl;
//    collectReachableMetaObjects(meta, metas);

//    for (int index = 0; index < meta->propertyCount(); ++index) {
//        QMetaProperty prop = meta->property(index);
//        if (QQmlMetaType::isQObject(prop.userType())) {
//            if (verbose)
//                std::cerr << "  Processing property" << qPrintable( prop.name() ) << std::endl;
//            currentProperty = QString("%1::%2").arg(meta->className(), prop.name());

//            // if the property was not initialized during construction,
//            // accessing a member of oo is going to cause a segmentation fault
//            QObject *oo = QQmlMetaType::toQObject(prop.read(object));
//            if (oo && !metas->contains(oo->metaObject()))
//                collectReachableMetaObjects(oo, metas);
//            currentProperty.clear();
//        }
//    }
//}

//void collectReachableMetaObjects(const QQmlType *ty, QSet<const QMetaObject *> *metas)
//{
//    collectReachableMetaObjects(ty->metaObject(), metas, ty->isExtendedType());
////    if (ty->attachedPropertiesType())
////        collectReachableMetaObjects(ty->attachedPropertiesType(), metas);
//}

///* We want to add the MetaObject for 'Qt' to the list, this is a
//   simple way to access it.
//*/
//class FriendlyQObject: public QObject
//{
//public:
//    static const QMetaObject *qtMeta() { return &staticQtMetaObject; }
//};

/* When we dump a QMetaObject, we want to list all the types it is exported as.
   To do this, we need to find the QQmlTypes associated with this
   QMetaObject.
*/
static QHash<QByteArray, QSet<const QQmlType *> > qmlTypesByCppName;

// No different versioning possible for a composite type.
//static QMap<QString, const QQmlType * > qmlTypesByCompositeName;

static QHash<QByteArray, QByteArray> cppToId;

/* Takes a C++ type name, such as Qt::LayoutDirection or QString and
   maps it to how it should appear in the description file.

   These names need to be unique globally, so we don't change the C++ symbol's
   name much. It is mostly used to for explicit translations such as
   QString->string and translations for extended QML objects.
*/
QByteArray convertToId(const QByteArray &cppName)
{
    return cppToId.value(cppName, cppName);
}

QByteArray convertToId(const QMetaObject *mo)
{
    QByteArray className(mo->className());
    if (!className.isEmpty())
        return convertToId(className);

    // likely a metaobject generated for an extended qml object
    if (mo->superClass()) {
        className = convertToId(mo->superClass());
        className.append("_extended");
        return className;
    }

    static QHash<const QMetaObject *, QByteArray> generatedNames;
    className = generatedNames.value(mo);
    if (!className.isEmpty())
        return className;

    std::cerr << "Found a QMetaObject without a className, generating a random name" << std::endl;
    className = QByteArray("error-unknown-name-");
    className.append(QByteArray::number(generatedNames.size()));
    generatedNames.insert(mo, className);
    return className;
}

//// Collect all metaobjects for types registered with qmlRegisterType() without parameters
//void collectReachableMetaObjectsWithoutQmlName( QSet<const QMetaObject *>& metas ) {
//    foreach (const QQmlType *ty, QQmlMetaType::qmlAllTypes()) {
//        if ( ! metas.contains(ty->metaObject()) ) {
//            if (!ty->isComposite()) {
//                collectReachableMetaObjects(ty, &metas);
//            } else {
//                qmlTypesByCompositeName[ty->elementName()] = ty;
//            }
//       }
//    }
//}

//QSet<const QMetaObject *> collectReachableMetaObjects(QQmlEngine *engine,
//                                                      QSet<const QMetaObject *> &noncreatables,
//                                                      QSet<const QMetaObject *> &singletons,
//                                                      const QList<QQmlType *> &skip = QList<QQmlType *>())
//{
//    QSet<const QMetaObject *> metas;
//    metas.insert(FriendlyQObject::qtMeta());

//    QHash<QByteArray, QSet<QByteArray> > extensions;
//    foreach (const QQmlType *ty, QQmlMetaType::qmlTypes()) {
//        if (!ty->isCreatable())
//            noncreatables.insert(ty->metaObject());
//        if (ty->isSingleton())
//            singletons.insert(ty->metaObject());
//        if (!ty->isComposite()) {
//            qmlTypesByCppName[ty->metaObject()->className()].insert(ty);
//            if (ty->isExtendedType())
//                extensions[ty->typeName()].insert(ty->metaObject()->className());
//            collectReachableMetaObjects(ty, &metas);
//        } else {
//            qmlTypesByCompositeName[ty->elementName()] = ty;
//        }
//    }

//    // Adjust exports of the base object if there are extensions.
//    // For each export of a base object there can be a single extension object overriding it.
//    // Example: QDeclarativeGraphicsWidget overrides the QtQuick/QGraphicsWidget export
//    //          of QGraphicsWidget.
//    foreach (const QByteArray &baseCpp, extensions.keys()) {
//        QSet<const QQmlType *> baseExports = qmlTypesByCppName.value(baseCpp);

//        const QSet<QByteArray> extensionCppNames = extensions.value(baseCpp);
//        foreach (const QByteArray &extensionCppName, extensionCppNames) {
//            const QSet<const QQmlType *> extensionExports = qmlTypesByCppName.value(extensionCppName);

//            // remove extension exports from base imports
//            // unfortunately the QQmlType pointers don't match, so can't use QSet::subtract
//            QSet<const QQmlType *> newBaseExports;
//            foreach (const QQmlType *baseExport, baseExports) {
//                bool match = false;
//                foreach (const QQmlType *extensionExport, extensionExports) {
//                    if (baseExport->qmlTypeName() == extensionExport->qmlTypeName()
//                            && baseExport->majorVersion() == extensionExport->majorVersion()
//                            && baseExport->minorVersion() == extensionExport->minorVersion()) {
//                        match = true;
//                        break;
//                    }
//                }
//                if (!match)
//                    newBaseExports.insert(baseExport);
//            }
//            baseExports = newBaseExports;
//        }
//        qmlTypesByCppName[baseCpp] = baseExports;
//    }

//    if (creatable) {
//        // find even more QMetaObjects by instantiating QML types and running
//        // over the instances
//        foreach (QQmlType *ty, QQmlMetaType::qmlTypes()) {
//            if (skip.contains(ty))
//                continue;
//            if (ty->isExtendedType())
//                continue;
//            if (!ty->isCreatable())
//                continue;
//            if (ty->typeName() == "QQmlComponent")
//                continue;

//            QString tyName = ty->qmlTypeName();
//            tyName = tyName.mid(tyName.lastIndexOf(QLatin1Char('/')) + 1);
//            if (tyName.isEmpty())
//                continue;

//            inObjectInstantiation = tyName;
//            QObject *object = 0;

//            if (ty->isSingleton()) {
//                QQmlType::SingletonInstanceInfo *siinfo = ty->singletonInstanceInfo();
//                if (!siinfo) {
//                    std::cerr << "Internal error, " << qPrintable(tyName)
//                              << "(" << qPrintable( QString::fromUtf8(ty->typeName()) ) << ")"
//                              << " is singleton, but has no singletonInstanceInfo" << std::endl;
//                    continue;
//                }
//                if (siinfo->qobjectCallback) {
//                    if (verbose)
//                        std::cerr << "Trying to get singleton for " << qPrintable(tyName)
//                                  << " (" << qPrintable( siinfo->typeName )  << ")" << std::endl;
//                    siinfo->init(engine);
//                    collectReachableMetaObjects(object, &metas);
//                    object = siinfo->qobjectApi(engine);
//                } else {
//                    inObjectInstantiation.clear();
//                    continue; // we don't handle QJSValue singleton types.
//                }
//            } else {
//                if (verbose)
//                    std::cerr << "Trying to create object " << qPrintable( tyName )
//                              << " (" << qPrintable( QString::fromUtf8(ty->typeName()) )  << ")" << std::endl;
//                object = ty->create();
//            }

//            inObjectInstantiation.clear();

//            if (object) {
//                if (verbose)
//                    std::cerr << "Got " << qPrintable( tyName )
//                              << " (" << qPrintable( QString::fromUtf8(ty->typeName()) ) << ")" << std::endl;
//                collectReachableMetaObjects(object, &metas);
//                object->deleteLater();
//            } else {
//                std::cerr << "Could not create" << qPrintable(tyName) << std::endl;
//            }
//        }
//    }

//    collectReachableMetaObjectsWithoutQmlName(metas);

//    return metas;
//}

class KnownAttributes {
    QHash<QByteArray, int> m_properties;
    QHash<QByteArray, QHash<int, int> > m_methods;
public:
    bool knownMethod(const QByteArray &name, int nArgs, int revision)
    {
        if (m_methods.contains(name)) {
            QHash<int, int> overloads = m_methods.value(name);
            if (overloads.contains(nArgs) && overloads.value(nArgs) <= revision)
                return true;
        }
        m_methods[name][nArgs] = revision;
        return false;
    }

    bool knownProperty(const QByteArray &name, int revision)
    {
        if (m_properties.contains(name) && m_properties.value(name) <= revision)
            return true;
        m_properties[name] = revision;
        return false;
    }
};


class Dumper
{
    QmlStreamWriter *qml;
    QString relocatableModuleUri;

public:
    Dumper(QmlStreamWriter *qml) : qml(qml) {}

    void setRelocatableModuleUri(const QString &uri)
    {
        relocatableModuleUri = uri;
    }

    const QString getExportString(QString qmlTyName, int majorVersion, int minorVersion)
    {
        if (qmlTyName.startsWith(relocatableModuleUri + QLatin1Char('/'))) {
            qmlTyName.remove(0, relocatableModuleUri.size() + 1);
        }
        if (qmlTyName.startsWith("./")) {
            qmlTyName.remove(0, 2);
        }
        if (qmlTyName.startsWith("/")) {
            qmlTyName.remove(0, 1);
        }
        const QString exportString = enquote(
                    QString("%1 %2.%3").arg(
                        qmlTyName,
                        QString::number(majorVersion),
                        QString::number(minorVersion)));
        return exportString;
    }

    void writeMetaContent(const QMetaObject *meta, KnownAttributes *knownAttributes = 0)
    {
        QSet<QString> implicitSignals;
        for (int index = meta->propertyOffset(); index < meta->propertyCount(); ++index) {
            const QMetaProperty &property = meta->property(index);
            dump(property, knownAttributes);
            if (knownAttributes)
                knownAttributes->knownMethod(QByteArray(property.name()).append("Changed"),
                                             0, property.revision());
            implicitSignals.insert(QString("%1Changed").arg(QString::fromUtf8(property.name())));
        }

        if (meta == &QObject::staticMetaObject) {
            // for QObject, hide deleteLater() and onDestroyed
            for (int index = meta->methodOffset(); index < meta->methodCount(); ++index) {
                QMetaMethod method = meta->method(index);
                QByteArray signature = method.methodSignature();
                if (signature == QByteArrayLiteral("destroyed(QObject*)")
                        || signature == QByteArrayLiteral("destroyed()")
                        || signature == QByteArrayLiteral("deleteLater()"))
                    continue;
                dump(method, implicitSignals, knownAttributes);
            }

            // and add toString(), destroy() and destroy(int)
            if (!knownAttributes || !knownAttributes->knownMethod(QByteArray("toString"), 0, 0)) {
                qml->writeStartObject(QLatin1String("Method"));
                qml->writeScriptBinding(QLatin1String("name"), enquote(QLatin1String("toString")));
                qml->writeEndObject();
            }
            if (!knownAttributes || !knownAttributes->knownMethod(QByteArray("destroy"), 0, 0)) {
                qml->writeStartObject(QLatin1String("Method"));
                qml->writeScriptBinding(QLatin1String("name"), enquote(QLatin1String("destroy")));
                qml->writeEndObject();
            }
            if (!knownAttributes || !knownAttributes->knownMethod(QByteArray("destroy"), 1, 0)) {
                qml->writeStartObject(QLatin1String("Method"));
                qml->writeScriptBinding(QLatin1String("name"), enquote(QLatin1String("destroy")));
                qml->writeStartObject(QLatin1String("Parameter"));
                qml->writeScriptBinding(QLatin1String("name"), enquote(QLatin1String("delay")));
                qml->writeScriptBinding(QLatin1String("type"), enquote(QLatin1String("int")));
                qml->writeEndObject();
                qml->writeEndObject();
            }
        } else {
            for (int index = meta->methodOffset(); index < meta->methodCount(); ++index)
                dump(meta->method(index), implicitSignals, knownAttributes);
        }
    }

    QString getPrototypeNameForCompositeType(const QMetaObject *metaObject, QSet<QByteArray> &defaultReachableNames,
                                             QList<const QMetaObject *> *objectsToMerge)
    {
        QString prototypeName;
        if (!defaultReachableNames.contains(metaObject->className())) {
            // dynamic meta objects can break things badly
            // but extended types are usually fine
            const QMetaObjectPrivate *mop = reinterpret_cast<const QMetaObjectPrivate *>(metaObject->d.data);
            if (!(mop->flags & DynamicMetaObject) && objectsToMerge
                    && !objectsToMerge->contains(metaObject))
                objectsToMerge->append(metaObject);
            const QMetaObject *superMetaObject = metaObject->superClass();
            if (!superMetaObject)
                prototypeName = "QObject";
            else
                prototypeName = getPrototypeNameForCompositeType(
                            superMetaObject, defaultReachableNames, objectsToMerge);
        } else {
            prototypeName = convertToId(metaObject->className());
        }
        return prototypeName;
    }

    void dumpComposite(QQmlEngine *engine, const QQmlType *compositeType, QSet<QByteArray> &defaultReachableNames)
    {
        QQmlComponent e(engine, compositeType->sourceUrl());
        if (!e.isReady()) {
            std::cerr << "WARNING: skipping module " << compositeType->elementName().toStdString()
                      << std::endl << e.errorString().toStdString() << std::endl;
            return;
        }

        QObject *object = e.create();

        if (!object)
            return;

        qml->writeStartObject("Component");

        const QMetaObject *mainMeta = object->metaObject();

        QList<const QMetaObject *> objectsToMerge;
        KnownAttributes knownAttributes;
        // Get C++ base class name for the composite type
        QString prototypeName = getPrototypeNameForCompositeType(mainMeta, defaultReachableNames,
                                                                 &objectsToMerge);
        qml->writeScriptBinding(QLatin1String("prototype"), enquote(prototypeName));

        QString qmlTyName = compositeType->qmlTypeName();
        // name should be unique
        qml->writeScriptBinding(QLatin1String("name"), enquote(qmlTyName));
        const QString exportString = getExportString(qmlTyName, compositeType->majorVersion(), compositeType->minorVersion());
        qml->writeArrayBinding(QLatin1String("exports"), QStringList() << exportString);
        qml->writeArrayBinding(QLatin1String("exportMetaObjectRevisions"), QStringList() << QString::number(compositeType->minorVersion()));
        qml->writeBooleanBinding(QLatin1String("isComposite"), true);

        if (compositeType->isSingleton()) {
            qml->writeBooleanBinding(QLatin1String("isCreatable"), false);
            qml->writeBooleanBinding(QLatin1String("isSingleton"), true);
        }

        for (int index = mainMeta->classInfoCount() - 1 ; index >= 0 ; --index) {
            QMetaClassInfo classInfo = mainMeta->classInfo(index);
            if (QLatin1String(classInfo.name()) == QLatin1String("DefaultProperty")) {
                qml->writeScriptBinding(QLatin1String("defaultProperty"), enquote(QLatin1String(classInfo.value())));
                break;
            }
        }

        foreach (const QMetaObject *meta, objectsToMerge)
            writeMetaContent(meta, &knownAttributes);

        qml->writeEndObject();
    }

    void dump(const QMetaObject *meta, bool isUncreatable, bool isSingleton)
    {
        qml->writeStartObject("Component");

        QByteArray id = convertToId(meta);
        qml->writeScriptBinding(QLatin1String("name"), enquote(id));

        for (int index = meta->classInfoCount() - 1 ; index >= 0 ; --index) {
            QMetaClassInfo classInfo = meta->classInfo(index);
            if (QLatin1String(classInfo.name()) == QLatin1String("DefaultProperty")) {
                qml->writeScriptBinding(QLatin1String("defaultProperty"), enquote(QLatin1String(classInfo.value())));
                break;
            }
        }

        if (meta->superClass())
            qml->writeScriptBinding(QLatin1String("prototype"), enquote(convertToId(meta->superClass())));

        QSet<const QQmlType *> qmlTypes = qmlTypesByCppName.value(meta->className());
        if (!qmlTypes.isEmpty()) {
            QHash<QString, const QQmlType *> exports;

            foreach (const QQmlType *qmlTy, qmlTypes) {
                const QString exportString = getExportString(qmlTy->qmlTypeName(), qmlTy->majorVersion(), qmlTy->minorVersion());
                exports.insert(exportString, qmlTy);
            }

            // ensure exports are sorted and don't change order when the plugin is dumped again
            QStringList exportStrings = exports.keys();
            std::sort(exportStrings.begin(), exportStrings.end());
            qml->writeArrayBinding(QLatin1String("exports"), exportStrings);

            if (isUncreatable)
                qml->writeBooleanBinding(QLatin1String("isCreatable"), false);

            if (isSingleton)
                qml->writeBooleanBinding(QLatin1String("isSingleton"), true);

            // write meta object revisions
            QStringList metaObjectRevisions;
            foreach (const QString &exportString, exportStrings) {
                int metaObjectRevision = exports[exportString]->metaObjectRevision();
                metaObjectRevisions += QString::number(metaObjectRevision);
            }
            qml->writeArrayBinding(QLatin1String("exportMetaObjectRevisions"), metaObjectRevisions);

            //HACK
//            if (const QMetaObject *attachedType = (*qmlTypes.begin())->attachedPropertiesType()) {
//                // Can happen when a type is registered that returns itself as attachedPropertiesType()
//                // because there is no creatable type to attach to.
//                if (attachedType != meta) {
//                    qml->writeScriptBinding(QLatin1String("attachedType"), enquote(
//                                                convertToId(attachedType)));
//                }
//            }
            //HACK-END [commented]
        }

        for (int index = meta->enumeratorOffset(); index < meta->enumeratorCount(); ++index)
            dump(meta->enumerator(index));

        writeMetaContent(meta);

        qml->writeEndObject();
    }

    void writeEasingCurve()
    {
        qml->writeStartObject(QLatin1String("Component"));
        qml->writeScriptBinding(QLatin1String("name"), enquote(QLatin1String("QEasingCurve")));
        qml->writeScriptBinding(QLatin1String("prototype"), enquote(QLatin1String("QQmlEasingValueType")));
        qml->writeEndObject();
    }

private:

    /* Removes pointer and list annotations from a type name, returning
       what was removed in isList and isPointer
    */
    static void removePointerAndList(QByteArray *typeName, bool *isList, bool *isPointer)
    {
        static QByteArray declListPrefix = "QQmlListProperty<";

        if (typeName->endsWith('*')) {
            *isPointer = true;
            typeName->truncate(typeName->length() - 1);
            removePointerAndList(typeName, isList, isPointer);
        } else if (typeName->startsWith(declListPrefix)) {
            *isList = true;
            typeName->truncate(typeName->length() - 1); // get rid of the suffix '>'
            *typeName = typeName->mid(declListPrefix.size());
            removePointerAndList(typeName, isList, isPointer);
        }

        *typeName = convertToId(*typeName);
    }

    void writeTypeProperties(QByteArray typeName, bool isWritable)
    {
        bool isList = false, isPointer = false;
        removePointerAndList(&typeName, &isList, &isPointer);

        qml->writeScriptBinding(QLatin1String("type"), enquote(typeName));
        if (isList)
            qml->writeScriptBinding(QLatin1String("isList"), QLatin1String("true"));
        if (!isWritable)
            qml->writeScriptBinding(QLatin1String("isReadonly"), QLatin1String("true"));
        if (isPointer)
            qml->writeScriptBinding(QLatin1String("isPointer"), QLatin1String("true"));
    }

    void dump(const QMetaProperty &prop, KnownAttributes *knownAttributes = 0)
    {
        int revision = prop.revision();
        QByteArray propName = prop.name();
        if (knownAttributes && knownAttributes->knownProperty(propName, revision))
            return;
        qml->writeStartObject("Property");
        qml->writeScriptBinding(QLatin1String("name"), enquote(QString::fromUtf8(prop.name())));
        if (revision)
            qml->writeScriptBinding(QLatin1String("revision"), QString::number(revision));
        writeTypeProperties(prop.typeName(), prop.isWritable());

        qml->writeEndObject();
    }

    void dump(const QMetaMethod &meth, const QSet<QString> &implicitSignals,
              KnownAttributes *knownAttributes = 0)
    {
        if (meth.methodType() == QMetaMethod::Signal) {
            if (meth.access() != QMetaMethod::Public)
                return; // nothing to do.
        } else if (meth.access() != QMetaMethod::Public) {
            return; // nothing to do.
        }

        QByteArray name = meth.name();
        const QString typeName = convertToId(meth.typeName());

        if (implicitSignals.contains(name)
                && !meth.revision()
                && meth.methodType() == QMetaMethod::Signal
                && meth.parameterNames().isEmpty()
                && typeName == QLatin1String("void")) {
            // don't mention implicit signals
            return;
        }

        int revision = meth.revision();
        if (knownAttributes && knownAttributes->knownMethod(name, meth.parameterNames().size(), revision))
            return;
        if (meth.methodType() == QMetaMethod::Signal)
            qml->writeStartObject(QLatin1String("Signal"));
        else
            qml->writeStartObject(QLatin1String("Method"));

        qml->writeScriptBinding(QLatin1String("name"), enquote(name));

        if (revision)
            qml->writeScriptBinding(QLatin1String("revision"), QString::number(revision));

        if (typeName != QLatin1String("void"))
            qml->writeScriptBinding(QLatin1String("type"), enquote(typeName));

        for (int i = 0; i < meth.parameterTypes().size(); ++i) {
            QByteArray argName = meth.parameterNames().at(i);

            qml->writeStartObject(QLatin1String("Parameter"));
            if (! argName.isEmpty())
                qml->writeScriptBinding(QLatin1String("name"), enquote(argName));
            writeTypeProperties(meth.parameterTypes().at(i), true);
            qml->writeEndObject();
        }

        qml->writeEndObject();
    }

    void dump(const QMetaEnum &e)
    {
        qml->writeStartObject(QLatin1String("Enum"));
        qml->writeScriptBinding(QLatin1String("name"), enquote(QString::fromUtf8(e.name())));

        QList<QPair<QString, QString> > namesValues;
        for (int index = 0; index < e.keyCount(); ++index) {
            namesValues.append(qMakePair(enquote(QString::fromUtf8(e.key(index))), QString::number(e.value(index))));
        }

        qml->writeScriptObjectLiteralBinding(QLatin1String("values"), namesValues);
        qml->writeEndObject();
    }
};


}// namespace


QPluginTypesFacade::QPluginTypesFacade(){

}

QPluginTypesFacade::~QPluginTypesFacade(){
}

QList<const QQmlType*> QPluginTypesFacade::extractTypes(const QString& module, QQmlEngine*){

    QList<const QQmlType*> types;
    foreach (const QQmlType *ty, QQmlMetaType::qmlTypes()){
        QString typeModule = ty->qmlTypeName();
        typeModule = typeModule.mid(0, typeModule.lastIndexOf(QLatin1Char('/')));

        if ( typeModule == module )
            types.append(ty);
    }
    return types;
}

QString QPluginTypesFacade::getTypeName(const QQmlType* type){
    return type->elementName();
}

void QPluginTypesFacade::extractPluginInfo(QByteArray* stream){
    QList<QQmlType*> types;
    QSet<const QMetaObject *> metas;

    foreach (const QQmlType *ty, QQmlMetaType::qmlTypes()){
//        qDebug() << ty->metaObject()->className();

        QString typeModule = ty->qmlTypeName();
        typeModule = typeModule.mid(0, typeModule.lastIndexOf(QLatin1Char('/')));

        if ( typeModule != "QtQuick" )
            qDebug() << ty->qmlTypeName() << ty->elementName() << ty->module() << ty->sourceUrl();


//        qDebug() << "Module:" << module;
//        qDebug() << "Id:" << convertToId(ty->metaObject());

        if ( typeModule == "QtQuick.Layouts" ){
            qmlTypesByCppName[convertToId(ty->metaObject())].insert(ty);
            metas.insert(ty->metaObject());
        }
//        qDebug() << ty->module();
    }

//    qDebug() << "TOTAL INSERTIONS:" << qmlTypesByCppName.size();
//    for( static QHash<QByteArray, QSet<const QQmlType *> >::iterator it = qmlTypesByCppName.begin();
//         it != qmlTypesByCppName.end(); ++it ){

//        qDebug() << "OBJECT:" << it.key();
//        qDebug() << "EXPORT COUNT:" << it.value().size();

//    }

//    QByteArray bytes;
//    QmlStreamWriter qml(&bytes);

//    qml.writeStartDocument();
//    qml.writeLibraryImport(QLatin1String("QtQuick.tooling"), 1, 2);
//    qml.write(QString("\n"
//              "// This file describes the plugin-supplied types contained in the library.\n"
//              "// It is used for QML tooling purposes only.\n"
//              "//\n"
//              "// This file was auto-generated by:\n"
//              "// '%1 %2'\n"
//              "\n").arg("Live CV").arg("1.2"));
//    qml.writeStartObject("Module");


//    QStringList quotedDependencies;
////    foreach (const QString &dep, dependencies) //TODO
////        quotedDependencies << enquote(dep);
//    qml.writeArrayBinding("dependencies", quotedDependencies);

//    QMap<QString, const QMetaObject *> nameToMeta;
//    foreach (const QMetaObject *meta, metas)
//        nameToMeta.insert(convertToId(meta), meta);

//    Dumper dumper(&qml);
////    dumper.setRelocatableModuleUri("QtQuick.Layouts");

//    foreach (const QMetaObject *meta, nameToMeta) {
//        dumper.dump(meta, true, false);
//    }
////    foreach (const QQmlType *compositeType, qmlTypesByCompositeName)
////        dumper.dumpComposite(&engine, compositeType, defaultReachableNames);

//    qml.writeEndObject();
//    qml.writeEndDocument();

//    std::cout << bytes.constData() << std::flush;

//    //TODO: See if object has any exports within this context

//    //TODO: Find dependencies and check wether all dependencies have been met
//    //TODO: Check out wether type exists in dependent libraries
//    //TODO: If type has exports -> theres no need to check, we can figure out from the exports
//    //TODO: IF type doesn't have exports -> check wether it exists already in the fakemetaobject system, if not it means
//    // it is part of the current library

////    foreach (const QMetaObject *mo, defaultReachable) {
////        qDebug() << mo->className();
////    }

////    qDebug() << defaultReachable.size();
////    qDebug() << defaultTypes.size();
}

}// namespace
