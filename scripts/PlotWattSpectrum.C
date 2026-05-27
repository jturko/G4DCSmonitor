double watt(double*x, double*p) {
    double E = x[0];
    double a = p[0];
    double b = p[1];
    double term1 = TMath::Exp(-E/a);
    double term2 = TMath::SinH(TMath::Sqrt(b*E));
    return (term1*term2);
}

void PlotWattSpectrum(double a = 0.988/*MeV*/, double b = 2.249/*1/MeV*/) {
    TF1* f = new TF1("fWatt", watt, 0, 10, 2);
    f->SetParameters(a,b);
    f->SetNpx(100000);
    f->Draw();
}
