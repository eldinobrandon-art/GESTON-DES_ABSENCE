#include <QApplication>
#include <QMainWindow>
#include <QTabWidget>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QSqlError>
#include <QTableView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QHeaderView>
#include <QPdfWriter>
#include <QPainter>
#include <QFileDialog>
#include <QPageSize>

static bool connexion() {
    QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL");
    db.setHostName("localhost");
    db.setDatabaseName("gestion_absences");
    db.setUserName("root");
    db.setPassword("exaucemuyah");
    if (!db.open()) {
        QMessageBox::critical(nullptr, "Erreur", "Connexion échouée");
        return false;
    }
    return true;
}

static QString mention(double taux) {
    if (taux < 10) return "ASSIDU";
    if (taux <= 20) return "AVERTISSEMENT";
    return "EXCLU DE L'EXAMEN";
}

static void rafraichir(QSqlQueryModel *m, QTableView *v, const QString &sql) {
    m->setQuery(sql);
    v->setModel(m);
    v->horizontalHeader()->setStretchLastSection(true);
}

// ---------- Application principale avec les 7 onglets ----------
class Fenetre : public QMainWindow {
public:
    Fenetre() {
        setWindowTitle("Gestion des absences des étudiants");
        resize(1000, 700);
        auto *tabs = new QTabWidget;
        tabs->addTab(ongletEtudiant(), "1. Étudiants");
        tabs->addTab(ongletMatiere(), "2. Matières");
        tabs->addTab(ongletAbsence(), "3. Absences");
        tabs->addTab(ongletFiche(), "4. Fiche d'assiduité");
        tabs->addTab(ongletRisque(), "5. Étudiants à risque");
        tabs->addTab(ongletClassement(), "6. Classement");
        auto *quit = new QWidget;
        tabs->addTab(quit, "7. Quitter");
        connect(tabs, &QTabWidget::currentChanged, [tabs](int i) {
            if (i == 6) qApp->quit();
        });
        setCentralWidget(tabs);
    }

private:
    static bool exec(const QString &sql, const QVariantList &p) {
        QSqlQuery q;
        q.prepare(sql);
        for (const auto &v : p) q.addBindValue(v);
        if (!q.exec()) {
            QMessageBox::warning(nullptr, "Erreur", q.lastError().text());
            return false;
        }
        return true;
    }
    static bool existe(const QString &sql, const QVariantList &p) {
        QSqlQuery q;
        q.prepare(sql);
        for (const auto &v : p) q.addBindValue(v);
        q.exec();
        return q.next() && q.value(0).toInt() > 0;
    }

    // ---------------- 1. ETUDIANT ----------------
    QWidget *ongletEtudiant() {
        auto *w = new QWidget; auto *l = new QVBoxLayout(w);
        auto *num = new QLineEdit; num->setPlaceholderText("N° inscription");
        auto *nom = new QLineEdit; nom->setPlaceholderText("Nom");
        auto *sexe = new QComboBox; sexe->addItems({"M", "F"});
        auto *niv = new QComboBox; niv->addItems({"L1","L2","L3","M1","M2"});
        auto *par = new QLineEdit; par->setPlaceholderText("Parcours");
        auto *rech = new QLineEdit; rech->setPlaceholderText("Rechercher par numéro ou nom");
        auto *filtre = new QComboBox; filtre->addItems({"Tous","L1","L2","L3","M1","M2"});
        auto *info = new QLabel;
        auto *vue = new QTableView; auto *mod = new QSqlQueryModel(w);
        auto *bAj = new QPushButton("Ajouter");
        auto *bMo = new QPushButton("Modifier niveau");
        auto *bSu = new QPushButton("Supprimer");
        auto *h1 = new QHBoxLayout; for (auto *x : QList<QWidget*>{num,nom,sexe,niv,par}) h1->addWidget(x);
        auto *h2 = new QHBoxLayout; for (auto *x : QList<QWidget*>{bAj,bMo,bSu}) h2->addWidget(x);
        auto *h3 = new QHBoxLayout; h3->addWidget(rech); h3->addWidget(filtre);
        for (auto *x : QList<QLayout*>{h1,h2,h3}) l->addLayout(x);
        l->addWidget(info); l->addWidget(vue);

        auto maj = [=]() {
            QString f = filtre->currentText();
            QString t = rech->text().replace("'", "''");
            QString sql = "SELECT * FROM ETUDIANT WHERE 1=1";
            if (f != "Tous") sql += " AND Niveau='" + f + "'";
            if (!t.isEmpty()) sql += " AND (nInscription LIKE '%" + t + "%' OR Nom LIKE '%" + t + "%')";
            rafraichir(mod, vue, sql);
            info->setText("Effectif : " + QString::number(mod->rowCount()));
        };
        maj();
        connect(rech, &QLineEdit::textChanged, maj);
        connect(filtre, &QComboBox::currentTextChanged, maj);
        connect(bAj, &QPushButton::clicked, [=]() {
            if (num->text().isEmpty() || nom->text().isEmpty()) return;
            if (existe("SELECT COUNT(*) FROM ETUDIANT WHERE nInscription=?", {num->text()})) {
                QMessageBox::information(w, "Info", "Etudiant déjà saisi"); return;
            }
            exec("INSERT INTO ETUDIANT VALUES (?,?,?,?,?)",
                 {num->text(), nom->text(), sexe->currentText(), niv->currentText(), par->text()});
            maj();
        });
        connect(bMo, &QPushButton::clicked, [=]() {
            if (!existe("SELECT COUNT(*) FROM ETUDIANT WHERE nInscription=?", {num->text()})) {
                QMessageBox::information(w, "Info", "Etudiant non trouvé"); return;
            }
            exec("UPDATE ETUDIANT SET Niveau=? WHERE nInscription=?", {niv->currentText(), num->text()});
            maj();
        });
        connect(bSu, &QPushButton::clicked, [=]() {
            if (!existe("SELECT COUNT(*) FROM ETUDIANT WHERE nInscription=?", {num->text()})) {
                QMessageBox::information(w, "Info", "Etudiant non trouvé"); return;
            }
            if (existe("SELECT COUNT(*) FROM ABSENCE WHERE nInscription=?", {num->text()})) {
                QMessageBox::information(w, "Info", "Suppression impossible : étudiant déjà utilisé"); return;
            }
            exec("DELETE FROM ETUDIANT WHERE nInscription=?", {num->text()});
            maj();
        });
        return w;
    }

    // ---------------- 2. MATIERE ----------------
    QWidget *ongletMatiere() {
        auto *w = new QWidget; auto *l = new QVBoxLayout(w);
        auto *code = new QLineEdit; code->setPlaceholderText("Code matière");
        auto *lib = new QLineEdit; lib->setPlaceholderText("Libellé");
        auto *vol = new QDoubleSpinBox; vol->setRange(1, 500); vol->setDecimals(0); vol->setSuffix(" h");
        auto *vue = new QTableView; auto *mod = new QSqlQueryModel(w);
        auto *bAj = new QPushButton("Ajouter");
        auto *bMo = new QPushButton("Modifier");
        auto *bSu = new QPushButton("Supprimer");
        auto *h1 = new QHBoxLayout; for (auto *x : QList<QWidget*>{code,lib,vol}) h1->addWidget(x);
        auto *h2 = new QHBoxLayout; for (auto *x : QList<QWidget*>{bAj,bMo,bSu}) h2->addWidget(x);
        l->addLayout(h1); l->addLayout(h2); l->addWidget(vue);
        auto maj = [=]() { rafraichir(mod, vue, "SELECT * FROM MATIERE"); };
        maj();
        connect(bAj, &QPushButton::clicked, [=]() {
            if (existe("SELECT COUNT(*) FROM MATIERE WHERE codeMatiere=?", {code->text()})) {
                QMessageBox::information(w, "Info", "Matière déjà saisie"); return;
            }
            exec("INSERT INTO MATIERE VALUES (?,?,?)", {code->text(), lib->text(), (int)vol->value()});
            maj();
        });
        connect(bMo, &QPushButton::clicked, [=]() {
            if (!existe("SELECT COUNT(*) FROM MATIERE WHERE codeMatiere=?", {code->text()})) {
                QMessageBox::information(w, "Info", "Matière non trouvée"); return;
            }
            exec("UPDATE MATIERE SET Libelle=?, VolumeHoraire=? WHERE codeMatiere=?",
                 {lib->text(), (int)vol->value(), code->text()});
            maj();
        });
        connect(bSu, &QPushButton::clicked, [=]() {
            if (existe("SELECT COUNT(*) FROM ABSENCE WHERE codeMatiere=?", {code->text()})) {
                QMessageBox::information(w, "Info", "Suppression impossible : matière déjà utilisée"); return;
            }
            if (!existe("SELECT COUNT(*) FROM MATIERE WHERE codeMatiere=?", {code->text()})) {
                QMessageBox::information(w, "Info", "Matière non trouvée"); return;
            }
            exec("DELETE FROM MATIERE WHERE codeMatiere=?", {code->text()});
            maj();
        });
        return w;
    }

    // ---------------- 3. ABSENCE ----------------
    QWidget *ongletAbsence() {
        auto *w = new QWidget; auto *l = new QVBoxLayout(w);
        auto *cE = new QComboBox; auto *cM = new QComboBox;
        QSqlQuery q1("SELECT nInscription FROM ETUDIANT ORDER BY 1");
        while (q1.next()) cE->addItem(q1.value(0).toString());
        QSqlQuery q2("SELECT codeMatiere FROM MATIERE ORDER BY 1");
        while (q2.next()) cM->addItem(q2.value(0).toString());
        auto *date = new QDateEdit(QDate::currentDate()); date->setCalendarPopup(true);
        date->setDisplayFormat("yyyy-MM-dd");
        auto *dur = new QDoubleSpinBox; dur->setRange(0.5, 24); dur->setSingleStep(0.5); dur->setSuffix(" h");
        auto *just = new QComboBox; just->addItems({"Non", "Oui"});
        auto *vue = new QTableView; auto *mod = new QSqlQueryModel(w);
        auto *bAj = new QPushButton("Saisir l'absence");
        auto *bSu = new QPushButton("Supprimer");
        auto *bAct = new QPushButton("Actualiser listes");
        auto *h1 = new QHBoxLayout; for (auto *x : QList<QWidget*>{cE,cM,date,dur,just}) h1->addWidget(x);
        auto *h2 = new QHBoxLayout; for (auto *x : QList<QWidget*>{bAj,bSu,bAct}) h2->addWidget(x);
        l->addLayout(h1); l->addLayout(h2); l->addWidget(vue);
        auto maj = [=]() { rafraichir(mod, vue, "SELECT * FROM ABSENCE ORDER BY dateSeance DESC"); };
        maj();
        connect(bAj, &QPushButton::clicked, [=]() {
            QString d = date->date().toString("yyyy-MM-dd");
            if (existe("SELECT COUNT(*) FROM ABSENCE WHERE nInscription=? AND codeMatiere=? AND dateSeance=?",
                       {cE->currentText(), cM->currentText(), d})) {
                QMessageBox::information(w, "Info", "Absence déjà saisie"); return;
            }
            exec("INSERT INTO ABSENCE VALUES (?,?,?,?,?)",
                 {cE->currentText(), cM->currentText(), d, dur->value(), just->currentText()});
            maj();
        });
        connect(bSu, &QPushButton::clicked, [=]() {
            exec("DELETE FROM ABSENCE WHERE nInscription=? AND codeMatiere=? AND dateSeance=?",
                 {cE->currentText(), cM->currentText(), date->date().toString("yyyy-MM-dd")});
            maj();
        });
        connect(bAct, &QPushButton::clicked, [=]() {
            cE->clear(); cM->clear();
            QSqlQuery a("SELECT nInscription FROM ETUDIANT ORDER BY 1");
            while (a.next()) cE->addItem(a.value(0).toString());
            QSqlQuery b("SELECT codeMatiere FROM MATIERE ORDER BY 1");
            while (b.next()) cM->addItem(b.value(0).toString());
            maj();
        });
        return w;
    }

    // ---------------- 4. FICHE D'ASSIDUITE (AVEC EXPORT PDF) ----------------
    QWidget *ongletFiche() {
        auto *w = new QWidget; auto *l = new QVBoxLayout(w);
        auto *num = new QLineEdit; num->setPlaceholderText("N° étudiant");
        auto *bOk = new QPushButton("Éditer la fiche");
        auto *bPdf = new QPushButton("Exporter en PDF"); // Bouton Bonus PDF
        auto *entete = new QLabel;
        auto *vue = new QTableView; auto *mod = new QSqlQueryModel(w);

        auto *h = new QHBoxLayout;
        h->addWidget(num);
        h->addWidget(bOk);
        h->addWidget(bPdf); // Ajout du bouton PDF dans la disposition

        l->addLayout(h);
        l->addWidget(entete);
        l->addWidget(vue);

        connect(bOk, &QPushButton::clicked, [=]() {
            QSqlQuery e; e.prepare("SELECT Nom, Niveau FROM ETUDIANT WHERE nInscription=?");
            e.addBindValue(num->text()); e.exec();
            if (!e.next()) { QMessageBox::information(w, "Info", "Etudiant non trouvé"); return; }
            entete->setText("N° ETUDIANT : " + num->text() + "   NOM : " + e.value(0).toString()
                            + "   NIVEAU : " + e.value(1).toString());
            QString n = num->text().replace("'", "''");
            QString sql =
                "SELECT M.codeMatiere AS Matiere, M.Libelle, M.VolumeHoraire AS Volume, "
                "IFNULL(SUM(CASE WHEN A.Justifiee='Non' THEN A.Duree END),0) AS HeuresNonJustifiees, "
                "ROUND(IFNULL(SUM(CASE WHEN A.Justifiee='Non' THEN A.Duree END),0)/M.VolumeHoraire*100,1) AS Taux, "
                "CASE WHEN IFNULL(SUM(CASE WHEN A.Justifiee='Non' THEN A.Duree END),0)/M.VolumeHoraire*100 < 10 THEN 'ASSIDU' "
                "WHEN IFNULL(SUM(CASE WHEN A.Justifiee='Non' THEN A.Duree END),0)/M.VolumeHoraire*100 <= 20 THEN 'AVERTISSEMENT' "
                "ELSE 'EXCLU DE L''EXAMEN' END AS Observation "
                "FROM MATIERE M LEFT JOIN ABSENCE A ON A.codeMatiere=M.codeMatiere AND A.nInscription='" + n + "' "
                      "GROUP BY M.codeMatiere, M.Libelle, M.VolumeHoraire";
            rafraichir(mod, vue, sql);
        });

        // Action du bouton d'export PDF (Bonus)
        connect(bPdf, &QPushButton::clicked, [=]() {
            if (num->text().isEmpty() || mod->rowCount() == 0) {
                QMessageBox::warning(w, "Attention", "Veuillez d'abord éditer la fiche d'un étudiant.");
                return;
            }

            QString fileName = QFileDialog::getSaveFileName(w, "Enregistrer la fiche en PDF", "", "Fichiers PDF (*.pdf)");
            if (fileName.isEmpty()) return;
            if (!fileName.endsWith(".pdf", Qt::CaseInsensitive)) fileName += ".pdf";

            QPdfWriter pdfWriter(fileName);
            pdfWriter.setPageSize(QPageSize(QPageSize::A4));
            pdfWriter.setResolution(300);

            QPainter painter;
            if (painter.begin(&pdfWriter)) {
                painter.setFont(QFont("Arial", 16, QFont::Bold));
                painter.drawText(100, 150, "UNIVERSITÉ - GESTION DES ABSENCES");

                painter.setFont(QFont("Arial", 12));
                painter.drawText(100, 250, entete->text());

                painter.drawLine(100, 280, 2300, 280);

                int y = 350;
                painter.setFont(QFont("Arial", 10, QFont::Bold));
                painter.drawText(100, y, "Matière");
                painter.drawText(600, y, "Libellé");
                painter.drawText(1200, y, "Volume");
                painter.drawText(1600, y, "Abs. (h)");
                painter.drawText(2000, y, "Taux (%)");

                y += 60;
                painter.setFont(QFont("Arial", 10));

                for (int row = 0; row < mod->rowCount(); ++row) {
                    QString mat = mod->data(mod->index(row, 0)).toString();
                    QString lib = mod->data(mod->index(row, 1)).toString();
                    QString vol = mod->data(mod->index(row, 2)).toString();
                    QString abs = mod->data(mod->index(row, 3)).toString();
                    QString taux = mod->data(mod->index(row, 4)).toString();

                    painter.drawText(100, y, mat);
                    painter.drawText(600, y, lib);
                    painter.drawText(1200, y, vol);
                    painter.drawText(1600, y, abs);
                    painter.drawText(2000, y, taux + "%");

                    y += 50;
                }

                painter.end();
                QMessageBox::information(w, "Succès", "La fiche d'assiduité a été exportée en PDF avec succès !");
            } else {
                QMessageBox::critical(w, "Erreur", "Impossible de créer le fichier PDF.");
            }
        });

        return w;
    }

    // ---------------- 5. ETUDIANTS A RISQUE (> 20 %) ----------------
    QWidget *ongletRisque() {
        auto *w = new QWidget; auto *l = new QVBoxLayout(w);
        auto *cM = new QComboBox;
        QSqlQuery q("SELECT codeMatiere FROM MATIERE ORDER BY 1");
        while (q.next()) cM->addItem(q.value(0).toString());
        auto *vue = new QTableView; auto *mod = new QSqlQueryModel(w);
        l->addWidget(cM); l->addWidget(vue);
        auto maj = [=]() {
            QString sql =
                "SELECT E.nInscription, E.Nom, E.Niveau, SUM(A.Duree) AS HeuresNonJustifiees, "
                "ROUND(SUM(A.Duree)/M.VolumeHoraire*100,1) AS Taux "
                "FROM ABSENCE A JOIN ETUDIANT E ON E.nInscription=A.nInscription "
                "JOIN MATIERE M ON M.codeMatiere=A.codeMatiere "
                "WHERE A.Justifiee='Non' AND A.codeMatiere='" + cM->currentText() + "' "
                                      "GROUP BY E.nInscription, E.Nom, E.Niveau, M.VolumeHoraire "
                                      "HAVING SUM(A.Duree)/M.VolumeHoraire*100 > 20 ORDER BY Taux DESC";
            rafraichir(mod, vue, sql);
        };
        maj();
        connect(cM, &QComboBox::currentTextChanged, maj);
        return w;
    }

    // ---------------- 6. CLASSEMENT ----------------
    QWidget *ongletClassement() {
        auto *w = new QWidget; auto *l = new QVBoxLayout(w);
        auto *cM = new QComboBox;
        QSqlQuery q("SELECT codeMatiere FROM MATIERE ORDER BY 1");
        while (q.next()) cM->addItem(q.value(0).toString());
        auto *vue = new QTableView; auto *mod = new QSqlQueryModel(w);
        l->addWidget(cM); l->addWidget(vue);
        auto maj = [=]() {
            QString sql =
                "SELECT E.nInscription, E.Nom, SUM(A.Duree) AS HeuresNonJustifiees "
                "FROM ABSENCE A JOIN ETUDIANT E ON E.nInscription=A.nInscription "
                "WHERE A.Justifiee='Non' AND A.codeMatiere='" + cM->currentText() + "' "
                                      "GROUP BY E.nInscription, E.Nom ORDER BY HeuresNonJustifiees DESC";
            rafraichir(mod, vue, sql);
        };
        maj();
        connect(cM, &QComboBox::currentTextChanged, maj);
        return w;
    }
};

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    if (!connexion()) return 1;
    Fenetre f;
    f.show();
    return a.exec();
}