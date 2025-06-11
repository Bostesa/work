import React from 'react';
import { useNavigate } from 'react-router-dom';
import EnoLogo from './components/EnoLogo';

const HomePage = () => {
  const navigate = useNavigate();

  const openChatbot = () => {
    navigate('/chatbot');
  };

  return (
    <div className="home-page">
      <div className="container">
        <header className="hero-header">
          <h1>Welcome to Eno Assistant</h1>
          <p>Your intelligent banking companion</p>
        </header>
        
        <main className="main-content">
          <div className="hero-section">
            <h2>Banking Made Simple</h2>
            <p>Get instant help with your banking needs through our AI-powered assistant Eno.</p>
            
            <div className="features">
              <div className="feature-card">
                <h3>Account Management</h3>
                <p>Check balances, view transactions, and manage your accounts</p>
              </div>
              <div className="feature-card">
                <h3>Bill Pay</h3>
                <p>Set up and manage your bill payments effortlessly</p>
              </div>
              <div className="feature-card">
                <h3>Financial Insights</h3>
                <p>Get personalized insights about your spending habits</p>
              </div>
            </div>
          </div>
        </main>
        
        {/* Eno Chatbot Logo - Capital One Style */}
        <EnoLogo onClick={openChatbot} />
      </div>
    </div>
  );
};

export default HomePage;