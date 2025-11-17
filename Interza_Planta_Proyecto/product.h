#ifndef PRODUCT_H
#define PRODUCT_H

#include <QString>

class Product {
public:
    Product(int id=0, const QString &type = "Default") : id(id), type(type), currentState(0) {}

    int id;
    QString type;
    int currentState;

    void advanceState() { currentState++; }
    QString showInfo() const {
        return QString("Product %1 Type:%2 State:%3").arg(id).arg(type).arg(currentState);
    }
};

#endif // PRODUCT_H
